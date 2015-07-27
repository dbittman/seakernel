#include <sea/dm/block.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/lib/queue.h>
static struct queue lru;

void block_cache_init(void)
{
	queue_create(&lru, 0);
}

void dm_block_cache_reclaim(void)
{
	struct buffer *br = queue_dequeue(&lru);
	mutex_acquire(&br->bd->cachelock);
	if(br->refs > 1 || (br->flags & BUFFER_DIRTY)) {
		mutex_release(&br->bd->cachelock);
		queue_enqueue_item(&lru, &br->qi, br);
		return;
	}

	hash_table_delete_entry(&br->bd->cache, &br->block, sizeof(br->block), 1);
	mutex_release(&br->bd->cachelock);
	buffer_put(br);
}

int dm_block_cache_insert(blockdevice_t *bd, uint64_t block, struct buffer *buf, int flags)
{
	mutex_acquire(&bd->cachelock);

	struct buffer *prev = 0;
	int exist = hash_table_get_entry(&bd->cache, &block, sizeof(block), 1, &prev) == 0;

	if(exist && !(flags & BLOCK_CACHE_OVERWRITE)) {
		mutex_release(&bd->cachelock);
		return -EEXIST;
	}

	if(exist) {
		hash_table_delete_entry(&bd->cache, &block, sizeof(block), 1);
	}
	hash_table_set_entry(&bd->cache, &block, sizeof(block), 1, buf);

	buffer_inc_refcount(buf);
	mutex_release(&bd->cachelock);
	queue_enqueue_item(&lru, &buf->qi, buf);
	if(exist) {
		/* overwritten, so will not writeback */
		queue_remove(&lru, &prev->qi);
		buffer_put(prev);
	}
	return 0;
}

struct buffer *dm_block_cache_get(blockdevice_t *bd, uint64_t block)
{
	mutex_acquire(&bd->cachelock);

	struct buffer *e;
	if(hash_table_get_entry(&bd->cache, &block, sizeof(block), 1, &e) == -ENOENT) {
		mutex_release(&bd->cachelock);
		return 0;
	}

	buffer_inc_refcount(e);
	mutex_release(&bd->cachelock);
	queue_remove(&lru, &e->qi);
	queue_enqueue_item(&lru, &e->qi, e);
	return e;
}

int block_cache_request(struct ioreq *req, off_t initial_offset, size_t total_bytecount, char *buffer)
{
	struct llist list;
	ll_create_lockless(&list);
	size_t block = req->block;
	size_t bytecount = total_bytecount;
	int numread = block_cache_get_bufferlist(&list, req);
	if(numread != req->count)
		return 0;

	struct llistnode *ln;
	struct buffer *br;
	ll_for_each_entry(&list, ln, struct buffer *, br) {
		off_t offset = 0;
		if(br->block == block) {
			offset = initial_offset;
		}
		size_t copy_amount = req->bd->blksz - offset;
		if(copy_amount > bytecount)
			copy_amount = bytecount;
		size_t position = (br->block - block) * req->bd->blksz;
		memcpy(buffer + position, br->data + offset, copy_amount);
		bytecount -= copy_amount;
		position += copy_amount;
		buffer_put(br);
	}
	return total_bytecount;
}

