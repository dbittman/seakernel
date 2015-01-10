#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>
#include <sea/cpu/atomic.h>

struct hash_table *icache;
struct llist *ic_inuse;
struct queue *ic_lru;

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

}

struct inode *vfs_inode_create()
{
	
}

void vfs_inode_get(struct inode *node)
{
	assert(add_atomic(&node->count, 1) > 1);
	assert((node->flags & INODE_INUSE));
}

struct inode *vfs_icache_get(int fs_idx, int sb_idx, uint32_t num)
{
	/* create if it doesn't exist */
	struct inode *node;
	int newly_created = 0;
	uint32_t key[3] = {fs_idx, sb_idx, num};
	if(hash_table_get_entry(icache, key, sizeof(uint32_t), 3, &node) == -ENOENT) {
		/* didn't find it. Okay, create one */
		node = vfs_inode_create();
		node->flags |= INODE_NEEDREAD;
		struct inode *ret = hash_table_set_or_get_entry(icache, key, sizeof(uint32_t), 3, node);
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
	if(!(ff_or_atomic(&node->flags, INODE_INUSE))) {
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

