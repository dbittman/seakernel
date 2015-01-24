#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/cpu/atomic.h>

int fs_unlink(struct inode *node, const char *name, size_t namelen)
{
	if(!vfs_inode_check_permissions(node, MAY_WRITE, 0))
		return -EACCES;
	struct dirent *dir = fs_dirent_lookup(node, name, namelen);
	if(!dir)
		return -ENOENT;
	rwlock_acquire(&dir->lock, RWL_WRITER);
	dir->flags |= DIRENT_UNLINK;
	rwlock_release(&dir->lock, RWL_WRITER);
	vfs_dirent_release(dir);
	return 0;
}

int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen)
{
	rwlock_acquire(&dir->lock, RWL_WRITER);
	if(!vfs_inode_check_permissions(dir, MAY_WRITE, 0))
		return -EACCES;
	int r = fs_callback_inode_link(dir, target, name, namelen);
	rwlock_release(&dir->lock, RWL_WRITER);
	if(r)
		return r;
	return 0;
}

