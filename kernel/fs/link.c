#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/dir.h>

/* note that this function doesn't actually delete the dirent. It just sets a flag that
 * says "hey, this was deleted". See vfs_dirent_release for more details. An important
 * aspect is that on unlinking a directory, it does unlink the . and .. entries, even
 * though the directory won't actually be deleted until vfs_dirent_release gets called
 * and the last reference is released. */
static int do_fs_unlink(struct inode *node, const char *name, size_t namelen, int rec)
{
	if(!vfs_inode_check_permissions(node, MAY_WRITE, 0))
		return -EACCES;
	struct dirent *dir = fs_dirent_lookup(node, name, namelen);
	if(!dir)
		return -ENOENT;
	struct inode *target = fs_dirent_readinode(dir, 1);
	if(S_ISDIR(target->mode) && rec) {
		do_fs_unlink(target, "..", 2, 0);
		do_fs_unlink(target, ".", 1, 0);
	}
	or_atomic(&dir->flags, DIRENT_UNLINK);
	vfs_dirent_release(dir);
	return 0;
}

int fs_unlink(struct inode *node, const char *name, size_t namelen)
{
	return do_fs_unlink(node, name, namelen, 1);
}

int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen)
{
	if(!vfs_inode_check_permissions(dir, MAY_WRITE, 0))
		return -EACCES;
	if(!S_ISDIR(dir->mode))
		return -ENOTDIR;
	rwlock_acquire(&dir->lock, RWL_WRITER);
	rwlock_acquire(&target->metalock, RWL_WRITER);
	int r = fs_callback_inode_link(dir, target, name, namelen);
	if(!r)
		add_atomic(&target->nlink, 1);
	rwlock_release(&target->metalock, RWL_WRITER);
	rwlock_release(&dir->lock, RWL_WRITER);
	if(r)
		return r;
	return 0;
}

