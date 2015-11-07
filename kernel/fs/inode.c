#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>
#include <stdatomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/reclaim.h>
#include <sea/vsprintf.h>
#include <sea/fs/dir.h>
#include <sea/fs/pipe.h>
#include <sea/fs/kerfs.h>

struct hash *icache;
struct linkedlist *ic_dirty, *ic_inuse;
struct queue *ic_lru;
struct mutex *ic_lock;

void vfs_icache_init(void)
{
	icache = hash_create(0, 0, 0x4000);

	ic_dirty = linkedlist_create(0, LINKEDLIST_MUTEX);
	ic_inuse = linkedlist_create(0, LINKEDLIST_MUTEX);
	ic_lru = queue_create(0, 0);
	ic_lock = mutex_create(0, 0);

	mm_reclaim_register(fs_inode_reclaim_lru, sizeof(struct inode));
}

/* these three just handle the dirent cache. They don't actually look anything up */
struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen)
{
	struct dirent *dir;
	assert(node->count);
	return hash_lookup(&node->dirents, name, namelen);
}

void vfs_inode_add_dirent(struct inode *node, struct dirent *dir)
{
	assert(node->count);
	hash_insert(&node->dirents, dir->name, dir->namelen, &dir->hash_elem, dir);
}

void vfs_inode_del_dirent(struct inode *node, struct dirent *dir)
{
	assert(node->count);
	hash_delete(&node->dirents, dir->name, dir->namelen);
}

int vfs_inode_check_permissions(struct inode *node, int perm, int real)
{
	uid_t u = real ? current_process->real_uid : current_process->effective_uid;
	gid_t g = real ? current_process->real_gid : current_process->effective_gid;
	if(u == 0)
		return 1;
	if(u == node->uid && (perm & node->mode))
		return 1;
	perm = perm >> 3;
	if(g == node->gid && (perm & node->mode))
		return 1;
	perm = perm >> 3;
	return perm & node->mode;
}

struct inode *vfs_inode_create (void)
{
	struct inode *node = kmalloc(sizeof(struct inode));
	rwlock_create(&node->lock);
	rwlock_create(&node->metalock);
	mutex_create(&node->mappings_lock, 0);

	hash_create(&node->dirents, 0, 1000);

	node->flags = INODE_INUSE;
	linkedlist_insert(ic_inuse, &node->inuse_item, node);

	return node;
}

/* you probably do not want to call this function directly. Use vfs_icache_put instead. */
void vfs_inode_destroy(struct inode *node)
{
	if(node->pipe) {
		fs_pipe_free(node);
	}
	rwlock_destroy(&node->lock);
	rwlock_destroy(&node->metalock);
	assert(!node->count);
	assert(!node->dirents.count);
	hash_destroy(&node->dirents);
	assert(!(node->flags & INODE_INUSE));
	fs_inode_destroy_physicals(node);
	kfree(node);
}

/* increase a refcount on an inode which is already owned (has a non-zero refcount). */
void vfs_inode_get(struct inode *node)
{
	mutex_acquire(ic_lock);
	atomic_fetch_add(&node->count, 1);
	assert(node->count > 1);
	assert((node->flags & INODE_INUSE));
	mutex_release(ic_lock);
}

/* read in an inode from the inode cache, OR pull it in from the FS */
/* TODO: can this fail? */
struct inode *vfs_icache_get(struct filesystem *fs, uint32_t num)
{
	/* create if it doesn't exist */
	struct inode *node;
	assert(fs);
	int newly_created = 0;
	uint32_t key[2] = {fs->id, num};
	mutex_acquire(ic_lock);
	if((node = hash_lookup(icache, key, sizeof(key))) == NULL) {
		/* didn't find it. Okay, create one */
		node = vfs_inode_create();
		node->filesystem = fs;
		node->flags = INODE_NEEDREAD;
		node->id = num;
		node->key[0] = fs->id;
		node->key[1] = num;
		hash_insert(icache, node->key, sizeof(node->key), &node->hash_elem, node);
		newly_created = 1;
	}
	assert(node->filesystem == fs);
	atomic_fetch_add(&node->count, 1);

	/* move to in-use */
	if(!(atomic_fetch_or(&node->flags, INODE_INUSE) & INODE_INUSE)) {
		atomic_fetch_add(&fs->usecount, 1);
		if(!newly_created) {
			if(!(node->flags & INODE_NOLRU))
				queue_remove(ic_lru, &node->lru_item);
			linkedlist_insert(ic_inuse, &node->inuse_item, node);
		}
	}
	fs_inode_pull(node);
	mutex_release(ic_lock);

	return node;
}

/* indicates that the inode needs to be read from the filesystem */
void vfs_inode_set_needread(struct inode *node)
{
	assert(!(node->flags & INODE_DIRTY));
	atomic_fetch_add(&node->flags, INODE_NEEDREAD);
}

/* indicates that the inode needs to be written back to the filesystem */
void vfs_inode_set_dirty(struct inode *node)
{
	assert(!(node->flags & INODE_NEEDREAD));
	if(!(atomic_fetch_or(&node->flags, INODE_DIRTY) & INODE_DIRTY)) {
		linkedlist_insert(ic_dirty, &node->dirty_item, node);
	}
}

/* indicates that an inode no longer needs to be written to the filesystem */
void vfs_inode_unset_dirty(struct inode *node)
{
	assert(node->flags & INODE_DIRTY);
	linkedlist_remove(ic_dirty, &node->dirty_item);
	atomic_fetch_and(&node->flags, ~INODE_DIRTY);
}

/* drop a reference to an inode. */
void vfs_icache_put(struct inode *node)
{
	assert(node->count > 0);
	mutex_acquire(ic_lock);
	if(atomic_fetch_sub(&node->count, 1) == 1) {
		assert(node->flags & INODE_INUSE);
		atomic_fetch_and(&node->flags, ~INODE_INUSE);
		if(node->filesystem) {
			atomic_fetch_sub(&node->filesystem->usecount, 1);
		}

		linkedlist_remove(ic_inuse, &node->inuse_item);
		if(node->flags & INODE_NOLRU) {
			assert(!(node->flags & INODE_INUSE));
			assert(!node->dirents.count);
			if(node->filesystem) {
				uint32_t key[2] = {node->filesystem->id, node->id};
				hash_delete(icache, key, sizeof(key));
			}
			fs_inode_push(node);
			vfs_inode_destroy(node);
		} else {
			queue_enqueue_item(ic_lru, &node->lru_item, node);
		}
	}
	mutex_release(ic_lock);
}

size_t fs_inode_reclaim_lru(void)
{
	int released = 0;
	mutex_acquire(ic_lock);
	struct queue_item *qi = queue_dequeue_item(ic_lru);
	if(!qi) {
		mutex_release(ic_lock);
		return 0;
	}
	struct inode *remove = qi->ent;
	assert(remove);
	/* there's a subtlety here: If this reclaim and the dirent reclaim
	 * run at the same time, there could be an issue. Since the inode
	 * in the dirent reclaim may have a zero-count, we have to make sure
	 * that it doesn't free the inode in the middle of the dirent being freed.
	 * that's why the rwlock is acquired in both. */
	rwlock_acquire(&remove->lock, RWL_WRITER);
	if(!remove->dirents.count) {
		assert(!remove->count);
		assert(!(remove->flags & INODE_INUSE));
		assert(!remove->dirents.count);
		uint32_t key[2] = {remove->filesystem->id, remove->id};
		hash_delete(icache, key, sizeof(key));
		fs_inode_push(remove);
		vfs_inode_destroy(remove);
		released = 1;
	} else {
		/* TODO: In theory, we should just free all of these, but I'm lazy */
		queue_enqueue_item(ic_lru, qi, remove);
		rwlock_release(&remove->lock, RWL_WRITER);
	}
	mutex_release(ic_lock);
	return released ? sizeof(struct inode) : 0;
}

/* read an inode from the filesystem */
int fs_inode_pull(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_NEEDREAD) {
		r = fs_callback_inode_pull(node);
		if(!r)
			atomic_fetch_and(&node->flags, ~INODE_NEEDREAD);
	}
	return r;
}

/* write an inode to a filesystem */
int fs_inode_push(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_DIRTY) {
		r = fs_callback_inode_push(node);
		if(!r)
			vfs_inode_unset_dirty(node);
	}
	return r;
}

void vfs_inode_mount(struct inode *node, struct filesystem *fs)
{
	assert(!node->mount);
	node->mount = fs;
	vfs_inode_get(node);
	fs->point = node;
}

void vfs_inode_umount(struct inode *node)
{
	assert(node->mount);
	node->mount->point = 0;
	node->mount = 0;
	vfs_icache_put(node);
}

ssize_t fs_inode_write(struct inode *node, size_t off, size_t count, const char *buf)
{
	if(S_ISDIR(node->mode))
		return -EISDIR;
	if(!vfs_inode_check_permissions(node, MAY_WRITE, 0))
		return -EACCES;
	ssize_t ret = fs_callback_inode_write(node, off, count, buf);
	if(ret > 0) {
		node->mtime = time_get_epoch();
		vfs_inode_set_dirty(node);
	}
	return ret;
}

ssize_t fs_inode_read(struct inode *node, size_t off, size_t count, char *buf)
{
	if(!vfs_inode_check_permissions(node, MAY_READ, 0))
		return -EACCES;
	ssize_t ret;

	ret = fs_callback_inode_read(node, off, count, buf);
	return ret;
}

int vfs_inode_chdir(struct inode *node)
{
	if(!S_ISDIR(node->mode))
		return -ENOTDIR;
	struct inode *old = current_process->cwd;
	current_process->cwd = node;
	vfs_inode_get(node);
	vfs_icache_put(old);
	return 0;
}

int vfs_inode_chroot(struct inode *node)
{
	if(!S_ISDIR(node->mode))
		return -ENOTDIR;
	if(current_process->effective_uid)
		return -EPERM;
	struct inode *old = current_process->root;
	current_process->root = node;
	vfs_inode_get(node);
	vfs_icache_put(old);
	return 0;
}

static void __icache_sync_action(struct linkedentry *entry)
{
	struct inode *node = entry->obj;
	if(node->flags & INODE_DIRTY)
		fs_callback_inode_push(node);
	linkedlist_do_remove(ic_dirty, &node->dirty_item);
	atomic_fetch_and(&node->flags, ~INODE_DIRTY);
}

/* it's important to sync the inode cache back to the disk... */
int fs_icache_sync(void)
{
	printk(0, "[fs]: syncing inode cache (%d)\n", ic_dirty->count);
	linkedlist_apply(ic_dirty, __icache_sync_action);
	printk(0, "\ndone\n");
	return 0;
}

int kerfs_icache_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"icache load: %d%%\n", (100 * hash_count(icache)) / hash_length(icache));
	KERFS_PRINTF(offset, length, buf, current,
			"IN USE %d, DIRTY %d, LRU LEN %d, TOTAL CACHED %d\n",
			ic_inuse->count, ic_dirty->count, ic_lru->count, icache->count);
	return current;
}

