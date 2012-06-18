#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

struct inode *do_lookup(struct inode *i, char *path, int aut, int ram, int *req)
{
	if(!i || !path || !*path)
		return 0;
	struct inode *temp;
	if(aut) {
		if(!strcmp(path, ".."))
		{
			if(i == current_task->root)
				return i;
			if(i->mount_parent)
				i = i->mount_parent;
			if(!i->parent || !i) 
				return 0;
			i = i->parent;
			return i;
		}
		if(!strcmp(path, ".")) {
			return i;
		}
	}
	/* Access? */
	if(!is_directory(i))
		return 0;
	temp = i->child;
	while(temp)
	{
		assert(!temp->unreal);
		/* Check to see if an inode is valid. This is similar to checks in 
		 * iput if it can be released If the validness fails, then the inode 
		 * could very well be being released */
		if(!strcmp(temp->name, path))
		{
			change_ireq(i, 1);
			if(req) *req=1;
			if(temp->mount)
				temp = temp->mount->root;
			/* Update info. We do this in case something inside the driver 
			 * has changed the stats of this file without us knowing. */
			vfs_callback_update(temp);
			return temp;
		}
		temp = temp->next;
	}
	/* Force Lookup */
	if(i->dynamic && i->i_ops && i->i_ops->lookup && !ram)
	{
		temp = vfs_callback_lookup(i, path);
		if(!temp)
			return 0;
		temp->count=1;
		temp->f_count=0;
		temp->required=0;
		add_inode(i, temp);
		return temp;
	}
	return 0;
}

struct inode *lookup(struct inode *i, char *path)
{
	int req=0;
	mutex_on(&i->lock);
	struct inode *ret = do_lookup(i, path, 1, 0, &req);
	mutex_off(&i->lock);
	if(ret) change_icount(ret, 1);
	if(req) change_ireq(i, -1);
	if(ret && S_ISLNK(ret->mode))
	{
		/* The link's actual contents contain the path to the linked file */
		char li[ret->len + 1];
		memset(li, 0, ret->len+1);
		read_fs(ret, 0, ret->len, li);
		struct inode *linked = get_idir(li, i);
		iput(ret);
		return linked;
	}
	return ret;
}

/* Returns a link if is the path specified */
struct inode *llookup(struct inode *i, char *path)
{
	int req=0;
	mutex_on(&i->lock);
	struct inode *ret = do_lookup(i, path, 1, 0, &req);
	mutex_off(&i->lock);
	if(ret) change_icount(ret, 1);
	if(req) change_ireq(i, -1);
	return ret;
}

struct inode *do_add_dirent(struct inode *p, char *name, int mode)
{
	if(!is_directory(p))
		return 0;
	if(p->mount) p = p->mount->root;
	mutex_on(&p->lock);
	if(!permissions(p, MAY_WRITE)) {
		mutex_off(&p->lock);
		return 0;
	}
	if(p->parent == current_task->root && !strcmp(p->name, "tmp"))
		mode |= 0x1FF;
	struct inode *ret = vfs_callback_create(p, name, mode);
	if(ret) {
		ret->mtime = get_epoch_time();
		ret->uid = current_task->uid;
		ret->gid = current_task->gid;
		sync_inode_tofs(ret);
		add_inode(p, ret);
	}
	mutex_off(&p->lock);
	return ret;
}

/* This function is the master path parser for the VFS. It will traverse
 * the given path and return an inode, or fail. Don't call this directly,
 * use the wrapper functions */
struct inode *do_get_idir(char *p_path, struct inode *b, int use_link, 
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
		from = current_task->pwd;
	/* Okay, path formatting. First check starting point */
	if(*path == '/') {
		from = current_task->root;
		++path;
	}
	if(!*path)
		return from;
	if(!from)
		return 0;
	struct inode *ret=from, *prev=0;
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
		/* Make sure we lookup inside a directory that a link is pointing to */
		if(use_link && S_ISLNK(ret->mode)) {
			/* The link's actual contents contain the path to the linked file */
			char li[ret->len + 1];
			memset(li, 0, ret->len+1);
			read_fs(ret, 0, ret->len, li);
			struct inode *linked = get_idir(li, prev);
			if(!linked)
				return 0;
			ret = linked;
		}
		struct inode *old = ret;
		if(use_link)
			ret = llookup(ret, current);
		else
			ret = lookup(ret, current);
		if(create && !ret) {
			ret = do_add_dirent(old, current, 
				dir_f ? (0x4000 | (create&0xFFF)) : ((create&0xFFF) | 0x8000));
			if(did_create && ret)
				*did_create=1;
			if(ret) ret->count++;
		}
		if(prev)
			change_icount(prev, -1);
		if(!ret)
			return 0;
		prev = ret;
		current=a;
	}
	return ret;
}
