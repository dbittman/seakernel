#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
int vfs_dirent_acquire(struct dirent *dir)
{
	add_atomic(&dir->count, 1);
	return 0;
}

int vfs_dirent_release(struct dirent *dir)
{
	int r = 0;
	if(!sub_atomic(&dir->count, 1)) {
		if(dir->flags & DIRENT_UNLINK) {
			//kprintf("--> FS UNLINK %s\n", dir->name);
			struct inode *target = fs_dirent_readinode(dir, 1);
			r = fs_callback_inode_unlink(dir->parent, dir->name, dir->namelen);
			if(!r) {
				vfs_inode_set_needread(target);
			}
		}
		vfs_inode_del_dirent(dir->parent, dir);
		vfs_icache_put(dir->parent);
	}
	/* TODO: reclaiming */

	return r;
}

struct dirent *vfs_dirent_create(struct inode *node)
{
	struct dirent *d = kmalloc(sizeof(struct dirent));
	rwlock_create(&d->lock);
	d->parent = node;
	d->filesystem = node->filesystem;
	return d;
}

void vfs_dirent_destroy(struct dirent *dir)
{
	assert(!dir->count);
	rwlock_destroy(&dir->lock);
	kfree(dir);
}

