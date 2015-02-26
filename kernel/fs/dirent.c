#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/fs/dir.h>
int vfs_dirent_acquire(struct dirent *dir)
{
	add_atomic(&dir->count, 1);
	return 0;
}

int vfs_dirent_release(struct dirent *dir)
{
	int r = 0;
	rwlock_acquire(&dir->parent->lock, RWL_WRITER);
	if(!sub_atomic(&dir->count, 1)) {
		if(dir->flags & DIRENT_UNLINK) {
			struct inode *target = fs_dirent_readinode(dir, 1);
			vfs_inode_del_dirent(dir->parent, dir);
			r = fs_callback_inode_unlink(dir->parent, dir->name, dir->namelen, target);
			if(!r) {
				assert(sub_atomic(&target->nlink, 1) >= 0);
				if(!target->nlink && (target->flags & INODE_DIRTY))
					vfs_inode_unset_dirty(target);
			}
		}
		rwlock_release(&dir->parent->lock, RWL_WRITER);
		vfs_icache_put(dir->parent);
	} else
		rwlock_release(&dir->parent->lock, RWL_WRITER);
	/* TODO: reclaiming */

	return r;
}

struct inode *fs_dirent_readinode(struct dirent *dir, int nofollow)
{
	assert(dir);
	if(!vfs_inode_check_permissions(dir->parent, MAY_EXEC, 0))
		return 0;
	struct inode *node = vfs_icache_get(dir->filesystem, dir->ino);
	assert(node);
	if(!nofollow && S_ISLNK(node->mode)) {
		size_t maxlen = node->length;
		if(maxlen > 1024)
			maxlen = 1024;
		char link[maxlen+1];
		if((size_t)fs_inode_read(node, 0, maxlen, link) != maxlen) {
			vfs_icache_put(node);
			return 0;
		}
		link[maxlen]=0;
		char *newpath = link;
		struct inode *start = dir->parent, *ln=0;
		if(link[0] == '/') {
			newpath++;
			start = current_task->thread->root;
		}
		int err;
		struct dirent *ln_dir = do_fs_path_resolve(start, link, &err);
		vfs_icache_put(node);
		if(ln_dir) {
			ln = fs_dirent_readinode(ln_dir, 0);
			vfs_dirent_release(ln_dir);
		} else {
			return 0;
		}
		node = ln;
	}
	return node;
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

