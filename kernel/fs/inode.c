#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>

struct hash_table *icache;
struct llist *ic_inuse, *ic_dirty;
struct queue *ic_lru;
mutex_t *ic_lock;

void vfs_icache_init()
{
	icache = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(icache, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(icache, HASH_FUNCTION_BYTE_SUM);

	ic_inuse = ll_create(0);
	ic_dirty = ll_create(0);
	ic_lru = queue_create(0, 0);
	ic_lock = mutex_create(0, 0);
}

struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen)
{
	struct dirent *dir;
	int r = hash_table_get_entry(node->dirents, (void *)name, 1, namelen, (void**)&dir);
	return r == -ENOENT ? 0 : dir;
}

void vfs_inode_add_dirent(struct inode *node, struct dirent *dir)
{
	int r = hash_table_set_entry(node->dirents, dir->name, 1, dir->namelen, dir);
	assert(!r);
}

void vfs_inode_del_dirent(struct inode *node, struct dirent *dir)
{
	int r = hash_table_delete_entry(node->dirents, dir->name, 1, dir->namelen);
	assert(!r);
}

int vfs_inode_check_permissions(struct inode *node, int perm, int real)
{
	uid_t u = real ? current_task->thread->real_uid : current_task->thread->effective_uid;
	gid_t g = real ? current_task->thread->real_gid : current_task->thread->effective_gid;
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

struct inode *vfs_inode_create()
{
	struct inode *node = kmalloc(sizeof(struct inode));
	rwlock_create(&node->lock);
	
	node->dirents = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(node->dirents, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(node->dirents, HASH_FUNCTION_BYTE_SUM);
	node->flags = INODE_INUSE;
	ll_do_insert(ic_inuse, &node->inuse_item, node);

	return node;
}

void vfs_inode_destroy(struct inode *node)
{
	rwlock_destroy(&node->lock);
	assert(!node->count);
	assert(!node->dirents->count);
	hash_table_destroy(node->dirents);
	assert(!(node->flags & INODE_INUSE));
	/* TODO: test queue */

	kfree(node);
}

void vfs_inode_get(struct inode *node)
{
	mutex_acquire(ic_lock);
	assert(add_atomic(&node->count, 1) > 1);
	assert((node->flags & INODE_INUSE));
	mutex_release(ic_lock);
}

struct inode *vfs_icache_get(struct filesystem *fs, uint32_t num)
{
	/* create if it doesn't exist */
	struct inode *node;
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
	add_atomic(&node->count, 1);
	
	/* move to in-use */
	if(!(ff_or_atomic(&node->flags, INODE_INUSE) & INODE_INUSE)) {
		if(!newly_created) {
			queue_remove(ic_lru, &node->lru_item);
			ll_do_insert(ic_inuse, &node->inuse_item, node);
		}
	}
	fs_inode_pull(node);
	mutex_release(ic_lock);

	return node;
}

void vfs_inode_set_needread(struct inode *node)
{
	assert(!(node->flags & INODE_DIRTY));
	or_atomic(&node->flags, INODE_NEEDREAD);
}

void vfs_inode_set_dirty(struct inode *node)
{
	assert(!(node->flags & INODE_NEEDREAD));
	if(!(ff_or_atomic(&node->flags, INODE_DIRTY) & INODE_DIRTY)) {
		assert(node->dirty_item.memberof == 0);
		ll_do_insert(ic_dirty, &node->dirty_item, node);
		assert(node->dirty_item.memberof == ic_dirty);
	}
}

void vfs_inode_unset_dirty(struct inode *node)
{
	assert(node->flags & INODE_DIRTY);
	assert(node->dirty_item.memberof == ic_dirty);
	ll_do_remove(ic_dirty, &node->dirty_item, 0);
	and_atomic(&node->flags, ~INODE_DIRTY);
}

void vfs_icache_put(struct inode *node)
{
	assert(node->count > 0);
	mutex_acquire(ic_lock);
	if(!sub_atomic(&node->count, 1)) {
		assert(node->flags & INODE_INUSE);
		and_atomic(&node->flags, ~INODE_INUSE);

		ll_do_remove(ic_inuse, &node->inuse_item, 0);
		queue_enqueue_item(ic_lru, &node->lru_item, node);
	}
	mutex_release(ic_lock);
}

#warning "locking?"
int fs_inode_pull(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_NEEDREAD) {
		r = fs_callback_inode_pull(node);
		if(!r)
			and_atomic(&node->flags, ~INODE_NEEDREAD);
	}
	return r;
}

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
	struct inode *old = current_task->thread->pwd;
	current_task->thread->pwd = node;
	vfs_inode_get(node);
	vfs_icache_put(old);
	return 0;
}

int vfs_inode_chroot(struct inode *node)
{
	if(!S_ISDIR(node->mode))
		return -ENOTDIR;
	if(current_task->thread->effective_uid)
		return -EPERM;
	struct inode *old = current_task->thread->root;
	current_task->thread->root = node;
	vfs_inode_get(node);
	vfs_icache_put(old);
	return 0;
}

int fs_icache_sync()
{
	printk(0, "[fs]: syncing inode cache (%d)\n", ic_dirty->num);
	rwlock_acquire(&ic_dirty->rwl, RWL_WRITER);
	struct llistnode *ln, *prev=0, *next;
	struct inode *node;
	ll_for_each_entry_safe(ic_dirty, ln, next, struct inode *, node) {
		assert(prev != ln);
		printk(0, "%x %x %d\n", ln, node, node->id);
		if(node->flags & INODE_DIRTY)
			fs_callback_inode_push(node);
		ll_do_remove(ic_dirty, &node->dirty_item, 1);
		and_atomic(&node->flags, ~INODE_DIRTY);
		prev = ln;
	}
	rwlock_release(&ic_dirty->rwl, RWL_WRITER);
	printk(0, "\ndone\n");
	return 0;
}

