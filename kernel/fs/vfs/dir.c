#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/dir.h>
#include <sea/fs/callback.h>
#include <sea/fs/proc.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
static int do_get_path_string(struct inode *p, char *path, int max)
{
	if(!p)
		return -EINVAL;
	if(max == -1)
		max = 2048;
	struct inode *i = p;
	if(!path)
		return -EINVAL;
	*path=0;
	if(!p)
		return 0;
	if(i != current_task->thread->root && i->mount_parent)
		i = i->mount_parent;
	char tmp[max * sizeof(char) +1];
	memset(tmp, 0, max * sizeof(char) +1);
	while(i && i->parent && ((int)(strlen(path) + 
		strlen(i->name)) < max || max == -1))
	{
		if(i->mount_parent)
			i = i->mount_parent;
		strncpy(tmp, path, max * sizeof(char) +1);
		snprintf(path, INAME_LEN, "%s/%s", i->name, tmp);
		i = i->parent;
		if(i == current_task->thread->root)
			break;
		if(i->mount_parent)
			i = i->mount_parent;
		if(i == current_task->thread->root)
			break;
	}
	strncpy(tmp, path, max * sizeof(char) +1);
	strncpy(path, "/", max - (strlen(tmp)+1));
	strncat(path, tmp, max);
	return 0;
}

int vfs_get_path_string(struct inode *p, char *buf, int len)
{
	if(!p || !buf || !len)
		return -EINVAL;
	return do_get_path_string(p, buf, len);
}

int sys_get_pwd(char *buf, int sz)
{
	if(!buf) 
		return -EINVAL;
	return do_get_path_string(current_task->thread->pwd, buf, sz == 0 ? -1 : sz);
}

int vfs_chroot(char *n)
{
	if(!n) 
		return -EINVAL;
	struct inode *i, *old = current_task->thread->root;
	if(current_task->thread->effective_uid != 0)
		return -EPERM;
	i = fs_resolve_path_inode(n, 0);
	if(!i)
		return -ENOENT;
	if(!vfs_inode_is_directory(i)) {
		vfs_icache_put(i);
		return -ENOTDIR;
	}
	current_task->thread->root = i;
	add_atomic(&i->count, 1);
	vfs_ichdir(i);
	vfs_icache_put(old);
	return 0;
}

int vfs_ichdir(struct inode *i)
{
	if(!i)
		return -EINVAL;
	struct inode *old=current_task->thread->pwd;
	if(!vfs_inode_is_directory(i)) {
		vfs_icache_put(i);
		return -ENOTDIR;
	}
	if(!vfs_inode_check_permissions(i, MAY_EXEC, 0)) {
		vfs_icache_put(i);
		return -EACCES;
	}
	current_task->thread->pwd = i;
	vfs_icache_put(old);
	return 0;
}

int vfs_chdir(char *path)
{
	if(!path) return -EINVAL;
	struct inode *i = fs_resolve_path_inode(path, 0);
	if(!i) return -ENOENT;
	return vfs_ichdir(i);
}

static struct inode *do_readdir(struct inode *i, int num)
{
	assert(i);
	int n = num;
	if(!vfs_inode_is_directory(i))
		return 0;
	struct inode *c=0;
	if(!i->dynamic) {
		rwlock_acquire(&i->rwl, RWL_READER);
		struct llistnode *cur;
		ll_for_each_entry((&i->children), cur, struct inode *, c)
		{
			if(!n--) break;
		}
		if(c)
			add_atomic(&c->count, 1);
		rwlock_release(&i->rwl, RWL_READER);
	}
	else if(i->i_ops && i->i_ops->readdir) {
		rwlock_acquire(&i->rwl, RWL_READER);
		if((c = vfs_callback_readdir(i, num)))
			c->count=1;
		rwlock_release(&i->rwl, RWL_READER);
	}
	return c;
}

struct inode *vfs_read_dir(char *n, int num)
{
	if(!n) return 0;
	struct inode *i=0;
	i = fs_resolve_path_inode(n, 0);
	if(!i)
		return 0;
	if(!vfs_inode_check_permissions(i, MAY_READ, 0)) {
		vfs_icache_put(i);
		return 0;
	}
	struct inode *ret = do_readdir(i, num);
	vfs_icache_put(i);
	return ret;
}

struct inode *vfs_read_idir(struct inode *i, int num)
{
	if(!i)
		return 0;
	if(!vfs_inode_check_permissions(i, MAY_READ, 0))
		return 0;
	return do_readdir(i, num);
}

int vfs_directory_is_empty(struct inode *i)
{
	if(!i || !vfs_inode_is_directory(i))
		return -EINVAL;
	if(inode_has_children(i)) return 1;
	struct inode *ret = do_readdir(i, 0);
	if(ret) vfs_icache_put(ret);
	return ret ? 0 : 1;
}

