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

struct hash_table *icache;
struct llist *ic_inuse, *ic_dirty;
struct queue *ic_lru;
mutex_t *ic_lock;

void vfs_icache_init(void)
{
	icache = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(icache, HASH_RESIZE_MODE_IGNORE,1000); /* TODO: initial value? */
	hash_table_specify_function(icache, HASH_FUNCTION_DEFAULT);

	ic_inuse = ll_create(0);
	ic_dirty = ll_create(0);
	ic_lru = queue_create(0, 0);
	ic_lock = mutex_create(0, 0);

	mm_reclaim_register(fs_inode_reclaim_lru, sizeof(struct inode));
}

/* these three just handle the dirent cache. They don't actually look anything up */
struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen)
{
	struct dirent *dir;
	assert(node->count);
	int r = hash_table_get_entry(node->dirents, (void *)name, 1, namelen, (void**)&dir);
	return r == -ENOENT ? 0 : dir;
}

void vfs_inode_add_dirent(struct inode *node, struct dirent *dir)
{
	assert(node->count);
	int r = hash_table_set_entry(node->dirents, dir->name, 1, dir->namelen, dir);
	assert(!r);
}

void vfs_inode_del_dirent(struct inode *node, struct dirent *dir)
{
	assert(node->count);
	int r = hash_table_delete_entry(node->dirents, dir->name, 1, dir->namelen);
	assert(!r);
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

	node->dirents = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(node->dirents, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(node->dirents, HASH_FUNCTION_DEFAULT);
	node->flags = INODE_INUSE;
	ll_do_insert(ic_inuse, &node->inuse_item, node);

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
	mutex_destroy(&node->mappings_lock);
	assert(!node->count);
	assert(!node->dirents->count);
	hash_table_destroy(node->dirents);
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
struct inode *vfs_icache_get(struct filesystem *fs, uint32_t num)
{
	/* create if it doesn't exist */
	struct inode *node;
	assert(fs);
	int newly_created = 0;
	uint32_t key[2] = {fs->id, num};
	mutex_acquire(ic_lock);
	if(hash_table_get_entry(icache, key, sizeof(uint32_t), 2, (void**)&node) == -ENOENT) {
		/* didn't find it. Okay, create one */
		node = vfs_inode_create();
		node->filesystem = fs;
		node->flags = INODE_NEEDREAD;
		node->id = num;
		hash_table_set_entry(icache, key, sizeof(uint32_t), 2, node);
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
			ll_do_insert(ic_inuse, &node->inuse_item, node);
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
		assert(node->dirty_item.memberof == 0);
		ll_do_insert(ic_dirty, &node->dirty_item, node);
		assert(node->dirty_item.memberof == ic_dirty);
	}
}

/* indicates that an inode no longer needs to be written to the filesystem */
void vfs_inode_unset_dirty(struct inode *node)
{
	assert(node->flags & INODE_DIRTY);
	assert(node->dirty_item.memberof == ic_dirty);
	ll_do_remove(ic_dirty, &node->dirty_item, 0);
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

		ll_do_remove(ic_inuse, &node->inuse_item, 0);
		if(node->flags & INODE_NOLRU) {
			assert(!(node->flags & INODE_INUSE));
			assert(!node->dirents->count);
			if(node->filesystem) {
				uint32_t key[2] = {node->filesystem->id, node->id};
				hash_table_delete_entry(icache, key, sizeof(uint32_t), 2);
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
	if(!remove->dirents->count) {
		assert(!remove->count);
		assert(!(remove->flags & INODE_INUSE));
		assert(!remove->dirents->count);
		uint32_t key[2] = {remove->filesystem->id, remove->id};
		hash_table_delete_entry(icache, key, sizeof(uint32_t), 2);
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
	return fs_callback_inode_read(node, off, count, buf);
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

/* it's important to sync the inode cache back to the disk... */
int fs_icache_sync(void)
{
	printk(0, "[fs]: syncing inode cache (%d)\n", ic_dirty->num);
	rwlock_acquire(&ic_dirty->rwl, RWL_WRITER);
	struct llistnode *ln, *prev=0, *next;
	struct inode *node;
	ll_for_each_entry_safe(ic_dirty, ln, next, struct inode *, node) {
		assert(prev != ln);
		printk(0, "%d\r", node->id);
		if(node->flags & INODE_DIRTY)
			fs_callback_inode_push(node);
		ll_do_remove(ic_dirty, &node->dirty_item, 1);
		atomic_fetch_and(&node->flags, ~INODE_DIRTY);
		prev = ln;
	}
	rwlock_release(&ic_dirty->rwl, RWL_WRITER);
	printk(0, "\ndone\n");
	return 0;
}

int kerfs_icache_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"    ID FSID FS-TYPE FLAGS REFCOUNT DIRENTS\n");
	rwlock_acquire(&ic_dirty->rwl, RWL_READER);
	struct llistnode *ln;
	struct inode *node;
	ll_for_each_entry(ic_inuse, ln, struct inode *, node) {
		KERFS_PRINTF(offset, length, buf, current,
				"%6d %4d %7s %5x %8d %7d",
				node->id, node->filesystem ? node->filesystem->id : -1,
				node->filesystem ? node->filesystem->type : "???",
				node->flags, node->count, node->dirents->count);
		struct dirent *dirent;
		int n = 0, used = 0;
		rwlock_acquire(&node->lock, RWL_READER);
		while(!hash_table_enumerate_entries(node->dirents, n++, 0, 0, 0, (void **)&dirent)) {
			if(dirent->count > 0)
				used++;
		}
		rwlock_release(&node->lock, RWL_READER);
		if(used > 0) {
			KERFS_PRINTF(offset, length, buf, current,
					" (%d)\n", used);
		} else {
			KERFS_PRINTF(offset, length, buf, current, "\n");
		}
	}
	rwlock_release(&ic_dirty->rwl, RWL_READER);
	KERFS_PRINTF(offset, length, buf, current,
			"    ID FSID FS-TYPE FLAGS REFCOUNT DIRENTS\n");
	KERFS_PRINTF(offset, length, buf, current,
			"IN USE %d, DIRTY %d, LRU LEN %d, TOTAL CACHED %d\n",
			ic_inuse->num, ic_dirty->num, ic_lru->count, icache->count);
	return current;
}

