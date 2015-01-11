#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
int vfs_dirent_acquire(struct dirent *dir)
{

}

int vfs_dirent_release(struct dirent *dir)
{
	if(!sub_atomic(&dir->count, 1))
		vfs_icache_put(dir->parent);
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

}

