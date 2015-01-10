#include <sea/fs/inode.h>

int vfs_dirent_acquire(struct dirent *dir)
{

}

int vfs_dirent_release(struct dirent *dir)
{
	if(!sub_atomic(&dir->count, 1))
		vfs_icache_put(dir->parent);
}

