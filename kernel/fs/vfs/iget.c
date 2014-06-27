#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
#include <sea/dm/pipe.h>
static struct inode *do_lookup(struct inode *i, char *path, int aut, int ram, int *req)
{
	if(!i || !path || !*path)
		return 0;
	struct inode *temp;
	if(aut) {
		if(!strcmp(path, ".."))
		{
			if(i == current_task->thread->root) {
				rwlock_acquire(&i->rwl, RWL_READER);
				add_atomic(&i->count, 1);
				rwlock_release(&i->rwl, RWL_READER);
				return i;
			}
			if(i->mount_parent)
				i = i->mount_parent;
			if(!i->parent || !i) 
				return 0;
			i = i->parent;
			rwlock_acquire(&i->rwl, RWL_READER);
			add_atomic(&i->count, 1);
			rwlock_release(&i->rwl, RWL_READER);
			return i;
		}
		if(!strcmp(path, ".")) {
			rwlock_acquire(&i->rwl, RWL_READER);
			add_atomic(&i->count, 1);
			rwlock_release(&i->rwl, RWL_READER);
			return i;
		}
	}
	/* Access? */
	if(!vfs_inode_is_directory(i))
		return 0;
	if(!vfs_inode_get_check_permissions(i, MAY_EXEC, 0))
		return 0;
	if(ll_is_active(&i->children)) {
		struct llistnode *cur;
		rwlock_acquire(&i->children.rwl, RWL_READER);
		ll_for_each_entry((&i->children), cur, struct inode *, temp)
		{
			/* Check to see if an inode is valid. This is similar to checks in 
			 * vfs_iput if it can be released If the validness fails, then the inode 
			 * could very well be being released */
			if(!strcmp(temp->name, path))
			{
				rwlock_release(&i->children.rwl, RWL_READER);
				if(temp->mount)
					temp = temp->mount->root;
				rwlock_acquire(&i->rwl, RWL_READER);
				add_atomic(&temp->count, 1);
				rwlock_release(&i->rwl, RWL_READER);
				/* Update info. We do this in case something inside the driver 
				 * has changed the stats of this file without us knowing. */
				vfs_callback_update(temp);
				return temp;
			}
		}
		rwlock_release(&i->children.rwl, RWL_READER);
	}
	/* Force Lookup */
	if(i->dynamic && i->i_ops && i->i_ops->lookup && !ram)
	{
		rwlock_acquire(&i->rwl, RWL_READER);
		temp = vfs_callback_lookup(i, path);
		if(!temp) {
			rwlock_release(&i->rwl, RWL_READER);
			return 0;
		}
		temp->count=1;
		vfs_do_add_inode(i, temp, 1);
		rwlock_release(&i->rwl, RWL_READER);
		return temp;
	}
	return 0;
}

/* auto-magically resolves links by calling iget again */
static struct inode *lookup(struct inode *i, char *path)
{
	int req=0;
	char unlock;
	rwlock_acquire(&i->rwl, RWL_READER);
	struct inode *ret = do_lookup(i, path, 1, 0, &req);
	rwlock_release(&i->rwl, RWL_READER);
	if(ret && S_ISLNK(ret->mode) && ret->len < 255)
	{
		/* The link's actual contents contain the path to the linked file */
		char li[ret->len + 1];
		memset(li, 0, ret->len+1);
		vfs_read_inode(ret, 0, ret->len, li);
		struct inode *linked = vfs_get_idir(li, i);
		vfs_iput(ret);
		return linked;
	}
	return ret;
}

/* Returns a link if is the path specified */
static struct inode *llookup(struct inode *i, char *path)
{
	int req=0;
	rwlock_acquire(&i->rwl, RWL_READER);
	struct inode *ret = do_lookup(i, path, 1, 0, &req);
	rwlock_release(&i->rwl, RWL_READER);
	return ret;
}

static struct inode *do_add_dirent(struct inode *p, char *name, int mode)
{
	if(!vfs_inode_is_directory(p))
		return 0;
	if(p->mount) p = p->mount->root;
	if(!vfs_inode_get_check_permissions(p, MAY_WRITE, 0))
		return 0;
	if(p->parent == current_task->thread->root && !strcmp(p->name, "tmp"))
		mode |= 0x1FF;
	rwlock_acquire(&p->rwl, RWL_WRITER);
	struct inode *ret = vfs_callback_create(p, name, mode);
	if(ret) ret->count=1;
	if(ret) {
		ret->mtime = arch_time_get_epoch();
		ret->uid = current_task->thread->effective_uid;
		ret->gid = current_task->thread->effective_gid;
		vfs_do_add_inode(p, ret, 1);
		rwlock_release(&p->rwl, RWL_WRITER);
		sync_inode_tofs(ret);
	} else
		rwlock_release(&p->rwl, RWL_WRITER);
	return ret;
}

/* This function is the master path parser for the VFS. It will traverse
 * the given path and return an inode, or fail. Don't call this directly,
 * use the wrapper functions */
struct inode *vfs_do_get_idir(char *p_path, struct inode *b, int use_link, 
	int create, int *did_create)
{
	if(did_create)
		*did_create=0;
	if(!p_path) return 0;
	int nplen=strlen(p_path)+4;
	char tmp_path[nplen];
	memset(tmp_path, 0, nplen);
	char *path = tmp_path;
	strncpy(path, p_path, nplen);
	struct inode *from=b;
	if(!from)
		from = current_task->thread->pwd;
	/* Okay, path formatting. First check starting point */
	if(*path == '/') {
		from = current_task->thread->root;
		++path;
	}
	if(!*path) {
		if(from) add_atomic(&from->count, 1);
		return from;
	}
	if(!from)
		return 0;
	add_atomic(&from->count, 1);
	struct inode *ret=from, *prev=from;
	char *a = path;
	char *current = path;
	int dir_f;
	while(current && *current)
	{
		a = strchr(current, '/');
		if(a == current)
		{
			++current;
			continue;
		}
		dir_f=0;
		if(a) {
			while(*a == '/') {
				dir_f=1;
				*(a++) = 0;
			}
		}
		if(!*current) break;
		/* Make sure we lookup inside a directory that a link is pointing to */
		if(use_link && S_ISLNK(ret->mode) && ret->len < 255) {
			/* The link's actual contents contain the path to the linked file */
			char li[ret->len + 1];
			memset(li, 0, ret->len+1);
			vfs_read_inode(ret, 0, ret->len, li);
			struct inode *linked = vfs_get_idir(li, prev);
			if(!linked) {
				if(prev) {
					rwlock_acquire(&prev->rwl, RWL_WRITER);
					sub_atomic(&prev->count, 1);
					rwlock_release(&prev->rwl, RWL_WRITER);
				}
				return 0;
			}
			ret = linked;
		}
		struct inode *old = ret;
		if(use_link)
			ret = llookup(ret, current);
		else
			ret = lookup(ret, current);
		if(create && !ret) {
			ret = do_add_dirent(old, current, 
				dir_f ? (0x4000|(create&0xFFF)) : ((create&0xFFF)|0x8000));
			if(did_create && ret)
				*did_create=1;
		}
		if(prev) {
			rwlock_acquire(&prev->rwl, RWL_WRITER);
			sub_atomic(&prev->count, 1);
			rwlock_release(&prev->rwl, RWL_WRITER);
		}
		if(!ret)
			return 0;
		prev = ret;
		current=a;
	}
	/* if its a named pipe, lets make sure that the pipe is really there */
	if(S_ISFIFO(ret->mode)) {
		if(!ret->pipe) {
			ret->pipe = dm_create_pipe();
			ret->pipe->type = PIPE_NAMED;
		}
	}
	return ret;
}

