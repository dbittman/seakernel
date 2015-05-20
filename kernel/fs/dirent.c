#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/fs/dir.h>

struct queue *dirent_lru;
mutex_t *dirent_cache_lock;

void vfs_dirent_init()
{
	dirent_lru = queue_create(0, 0);
	dirent_cache_lock = mutex_create(0, 0);
}

int vfs_dirent_acquire(struct dirent *dir)
{
	add_atomic(&dir->count, 1);
	return 0;
}

/* this function is called when a reference to a dirent is released. There is one
 * special thing to note: according to POSIX, an unlinked dirent is only actually
 * deleted when the last reference to it is dropped. Because of that, unlink just
 * sets a flag which tells this function to do the actual deletion. In either case,
 * this function doesn't deallocate anything, it just moves it to an LRU queue. */
int vfs_dirent_release(struct dirent *dir)
{
	int r = 0;
	//vfs_inode_get(dir->parent);
	struct inode *parent = dir->parent;
	rwlock_acquire(&parent->lock, RWL_WRITER);
	if(!sub_atomic(&dir->count, 1)) {
		if(dir->flags & DIRENT_UNLINK) {
			struct inode *target = fs_dirent_readinode(dir, 1);
			vfs_inode_del_dirent(parent, dir);
			r = fs_callback_inode_unlink(parent, dir->name, dir->namelen, target);
			if(!r) {
				assert(sub_atomic(&target->nlink, 1) >= 0);
				if(!target->nlink && (target->flags & INODE_DIRTY))
					vfs_inode_unset_dirty(target);
			}
			vfs_dirent_destroy(dir);
		} else {
			/* add to LRU */
			queue_enqueue_item(dirent_lru, &dir->lru_item, dir);
		}
		rwlock_release(&parent->lock, RWL_WRITER);
		//vfs_icache_put(parent); /* once for the parent pointer in this function */
		/* TODO: Do we do this here or in LRU reclaim? */
		vfs_icache_put(parent); /* for the dir->parent pointer */
	} else
		rwlock_release(&parent->lock, RWL_WRITER);

	return r;
}

void fs_dirent_remove_lru(struct dirent *dir)
{
	queue_remove(dirent_lru, &dir->lru_item);
}

void fs_dirent_reclaim_lru()
{
	mutex_acquire(dirent_cache_lock);
	struct queue_item *qi = queue_dequeue_item(dirent_lru);
	if(!qi) {
		mutex_release(dirent_cache_lock);
		return;
	}
	struct dirent *dir = qi->ent;
	struct inode *parent = dir->parent;
	vfs_inode_get(parent);
	rwlock_acquire(&parent->lock, RWL_WRITER);
	if(dir && dir->count == 0) {
		/* reclaim this node */
		vfs_inode_del_dirent(parent, dir);
		//vfs_icache_put(parent); /* for the dir->parent pointer */
		vfs_dirent_destroy(dir);
	}
	rwlock_release(&parent->lock, RWL_WRITER);
	vfs_icache_put(parent);
	mutex_release(dirent_cache_lock);
}

/* This function returns the directory entry associated with the name 'name' under
 * the inode 'node'. It must be careful to lookup the entry in the cache first. */
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen)
{
	if(!vfs_inode_check_permissions(node, MAY_READ, 0))
		return 0;
	if(!S_ISDIR(node->mode))
		return 0;
	if(node == current_task->thread->root && !strncmp(name, "..", 2) && namelen == 2)
		return fs_dirent_lookup(node, ".", 1);
	mutex_acquire(dirent_cache_lock);
	rwlock_acquire(&node->lock, RWL_WRITER);
	struct dirent *dir = vfs_inode_get_dirent(node, name, namelen);
	if(!dir) {
		dir = vfs_dirent_create(node);
		dir->count = 1;
		strncpy(dir->name, name, namelen);
		dir->namelen = namelen;
		int r = fs_callback_inode_lookup(node, name, namelen, dir);
		if(r) {
			dir->count = 0;
			vfs_dirent_destroy(dir);
			rwlock_release(&node->lock, RWL_WRITER);
			mutex_release(dirent_cache_lock);
			return 0;
		}
		vfs_inode_get(node);
		vfs_inode_add_dirent(node, dir);
	} else {
		if(add_atomic(&dir->count, 1) == 1) {
			fs_dirent_remove_lru(dir);
			//TODO: Pretty sure we don't need this here
			vfs_inode_get(node);
		}
	}
	rwlock_release(&node->lock, RWL_WRITER);
	mutex_release(dirent_cache_lock);
	return dir;
}

/* this entry returns the inode pointed to by the given directory entry. It also
 * handles redirecting for symbolic links */
struct inode *fs_dirent_readinode(struct dirent *dir, int nofollow)
{
	assert(dir);
	if(!vfs_inode_check_permissions(dir->parent, MAY_EXEC, 0))
		return 0;
	struct inode *node = vfs_icache_get(dir->filesystem, dir->ino);
	assert(node);
	if(!nofollow && S_ISLNK(node->mode)) {
		/* handle symbolic links */
		size_t maxlen = node->length;
		if(maxlen > 1024)
			maxlen = 1024;
		char link[maxlen+1];
		/* read in the link target */
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
		/* resolve the path. WARNING: TODO: this is currently
		 * recursive WITHOUT A LIMITATION. This needs to be fixed. */
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

