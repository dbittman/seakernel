#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>

struct hash_table *icache;
struct llist *ic_inuse;
struct queue *ic_lru;

void vfs_icache_init()
{
	icache = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(icache, HASH_RESIZE_MODE_IGNORE,100000);
	hash_table_specify_function(icache, HASH_FUNCTION_BYTE_SUM);

	ic_inuse = ll_create(0);
	ic_lru = queue_create(0, 0);
}

struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen)
{
	struct dirent *dir;
	int r = hash_table_get_entry(node->dirents, (void *)name, 1, namelen, &dir);
	return r == -ENOENT ? 0 : dir;
}

void vfs_inode_add_dirent(struct inode *node, struct dirent *dir)
{

}

void vfs_inode_del_dirent(struct inode *node, const char *name)
{

}

int vfs_inode_check_permissions(struct inode *node, int perm)
{
	return 1;
}

#warning "fix calls to this from other locations"
struct inode *vfs_inode_create()
{
	struct inode *node = kmalloc(sizeof(struct inode));
	rwlock_create(&node->lock);
	
	node->dirents = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(node->dirents, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(node->dirents, HASH_FUNCTION_BYTE_SUM);
#warning "TODO: other inits"

	return node;
}

void vfs_inode_destroy(struct inode *node)
{

}

void vfs_inode_get(struct inode *node)
{
	assert(add_atomic(&node->count, 1) > 1);
	assert((node->flags & INODE_INUSE));
}

struct inode *vfs_icache_get(struct filesystem *fs, uint32_t num)
{
	/* create if it doesn't exist */
	struct inode *node;
	int newly_created = 0;
	uint32_t key[3] = {fs->id, num};
	if(hash_table_get_entry(icache, key, sizeof(uint32_t), 3, &node) == -ENOENT) {
		kprintf("create new inode\n");
		/* didn't find it. Okay, create one */
		node = vfs_inode_create();
		node->filesystem = fs;
		node->flags |= INODE_NEEDREAD;
		node->id = num;
		struct inode *ret;
		hash_table_set_or_get_entry(icache, key, sizeof(uint32_t), 3, node, &ret);
		if(ret != node) {
			vfs_inode_destroy(node);
		} else {
			newly_created = 1;
		}
		/* someone else created this node before us. Okay. */
		node = ret;
	}
#warning "need to lock here? (for reclaiming)"
	add_atomic(&node->count, 1);
	
	/* move to in-use */
	if(!(ff_or_atomic(&node->flags, INODE_INUSE) & INODE_INUSE)) {
		kprintf("move to inuse\n");
		if(!newly_created)
			queue_remove(ic_lru, &node->lru_item);
		ll_do_insert(ic_inuse, &node->inuse_item, node);
	}

	return node;
}

void vfs_inode_set_dirty(struct inode *node)
{
	assert(!(node->flags & INODE_NEEDREAD));
	or_atomic(&node->flags, INODE_DIRTY);
}

void vfs_icache_put(struct inode *node)
{
	if(!sub_atomic(&node->count, 1)) {
		assert(node->flags & INODE_INUSE);
		and_atomic(&node->flags, ~INODE_INUSE);
		ll_do_remove(ic_inuse, &node->inuse_item, 0);
		queue_enqueue_item(ic_lru, &node->lru_item, node);
	}
}

void vfs_inode_mount(struct inode *node, struct inode *mount)
{

}

void vfs_inode_umount(struct inode *node)
{

}

int fs_inode_write(struct inode *node, size_t off, size_t count, const char *buf)
{

}

int fs_inode_read(struct inode *node, size_t off, size_t count, char *buf)
{

}

int vfs_inode_chdir(struct inode *node)
{

}

