#include <sea/dm/block.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/lib/queue.h>
#include <sea/mm/reclaim.h>
static struct queue lru;

size_t dm_block_cache_reclaim(void);
mutex_t reclaim_lock;
void block_cache_init(void)
{
	queue_create(&lru, 0);
	mm_reclaim_register(dm_block_cache_reclaim, sizeof(struct buffer) + 512);
	mutex_create(&reclaim_lock, 0);
}

size_t dm_block_cache_reclaim(void)
{
	/* TODO */
	return 0;
	mutex_acquire(&reclaim_lock);
	struct queue_item *item = queue_dequeue_item(&lru);
	struct buffer *br = item->ent;
	size_t amount = sizeof(struct buffer) + br->bd->blksz;
	mutex_acquire(&br->bd->cachelock);
	if((br->flags & BUFFER_DIRTY) || (br->flags & BUFFER_LOCKED)) {
		mutex_release(&br->bd->cachelock);
		queue_enqueue_item(&lru, &br->qi, br);
		mutex_release(&reclaim_lock);
		return 0;
	}

	hash_delete(&br->bd->cache, &br->block, sizeof(br->block));
	mutex_release(&br->bd->cachelock);
	buffer_put(br);
	mutex_release(&reclaim_lock);
	return amount;
}

int dm_block_cache_insert(blockdevice_t *bd, uint64_t block, struct buffer *buf, int flags)
{
	mutex_acquire(&bd->cachelock);

	struct buffer *prev = 0;
	bool exist = hash_lookup(&bd->cache, &block, sizeof(block)) != NULL;

	if(exist && !(flags & BLOCK_CACHE_OVERWRITE)) {
		mutex_release(&bd->cachelock);
		return -EEXIST;
	}

	buffer_inc_refcount(buf);
	if(exist) {
		hash_delete(&bd->cache, &block, sizeof(block));
	}
	buf->block = block;
	hash_insert(&bd->cache, &buf->block, sizeof(buf->block), &buf->hash_elem, buf);

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
	if((e = hash_lookup(&bd->cache, &block, sizeof(block))) == NULL) {
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
	size_t block = req->block;
	size_t bytecount = total_bytecount;
	while(req->count > 0) {
		struct buffer *br = block_cache_get_first_buffer(req);
		if(!br)
			return 0;
		req->count--;
		req->block++;
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

