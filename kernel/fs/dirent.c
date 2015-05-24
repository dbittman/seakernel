#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/reclaim.h>
#include <sea/fs/dir.h>

struct queue *dirent_lru;
mutex_t *dirent_cache_lock;

/* Refcounting correctness rules note:
 * The dirent->parent pointer may be invalid after
 * vfs_dirent_release drops the direct down to count zero.
 * Therefore, any time that the dir->parent pointer is accessed, 
 * it must be from a dirent that has a non-zero count, which
 * is required anyway.
 *
 * The only time a directory entry may be acquired from a time
 * where it had zero count is when the inode it's being gotten
 * from has a non-zero count. The only time an inode may be
 * read from a directory entry is when the dirent has a non-zero count.
 */

void vfs_dirent_init()
{
	dirent_lru = queue_create(0, 0);
	dirent_cache_lock = mutex_create(0, 0);
	mm_reclaim_register(fs_dirent_reclaim_lru, sizeof(struct dirent));
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
		/* So, here's the thing. Technically, we still have a pointer that points
		 * to parent: in dir->parent. We just have to make sure that each time
		 * we use this pointer, we don't screw up */
		vfs_icache_put(parent); /* for the dir->parent pointer */
	} else
		rwlock_release(&parent->lock, RWL_WRITER);

	return r;
}

void fs_dirent_remove_lru(struct dirent *dir)
{
	queue_remove(dirent_lru, &dir->lru_item);
}

size_t fs_dirent_reclaim_lru()
{
	mutex_acquire(dirent_cache_lock);
	struct queue_item *qi = queue_dequeue_item(dirent_lru);
	if(!qi) {
		mutex_release(dirent_cache_lock);
		return 0;
	}
	struct dirent *dir = qi->ent;
	struct inode *parent = dir->parent;
	rwlock_acquire(&parent->lock, RWL_WRITER);
	add_atomic(&parent->count, 1);
	if(dir && dir->count == 0) {
		/* reclaim this node */
		vfs_inode_del_dirent(parent, dir);
		vfs_dirent_destroy(dir);
	}
	sub_atomic(&parent->count, 1);
	rwlock_release(&parent->lock, RWL_WRITER);
	mutex_release(dirent_cache_lock);
	return sizeof(struct dirent);
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

