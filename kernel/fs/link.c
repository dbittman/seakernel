#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/cpu/atomic.h>

int fs_unlink(struct inode *node, const char *name, size_t namelen)
{
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
	int r = fs_callback_inode_link(dir, target, name, namelen);
#warning "need to set NEEDREAD after ops like this?"
	rwlock_release(&dir->lock, RWL_WRITER);
	if(r)
		return r;
	return 0;
}

