#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/cpu/atomic.h>

int fs_unlink(struct inode *node, const char *name, size_t namelen)
{
	struct dirent *dir = fs_dirent_lookup(node, name, namelen);
	if(!dir)
		return -ENOENT;
	struct inode *target = fs_dirent_readinode(dir, 1);
	if(!target)
		return -EIO;
	rwlock_acquire(&node->lock, RWL_WRITER);
#warning "dont-unlink-until-unused"
	kprintf("FS_UNLINK--> %d\n", target->count);
	int r=-EBUSY;
	if(target->count == 1)
	r = fs_callback_inode_unlink(node, name, namelen);
	if(!r) {
		vfs_inode_del_dirent(node, dir);
		rwlock_release(&node->lock, RWL_WRITER);
		rwlock_acquire(&target->lock, RWL_WRITER);
		sub_atomic(&target->nlink, 1);
		rwlock_release(&target->lock, RWL_WRITER);
	} else {
		rwlock_release(&node->lock, RWL_WRITER);
	}
	return r;
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

