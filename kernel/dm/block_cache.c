#include <sea/dm/block.h>
#include <sea/dm/blockdev.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/lib/queue.h>
#include <sea/mm/reclaim.h>
static struct queue lru;

size_t dm_block_cache_reclaim(void);
struct mutex reclaim_lock;
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
	size_t amount = sizeof(struct buffer) + br->bd->ctl->blocksize;
	mutex_acquire(&br->bd->ctl->cachelock);
	if((br->flags & BUFFER_DIRTY) || (br->flags & BUFFER_LOCKED)) {
		mutex_release(&br->bd->ctl->cachelock);
		queue_enqueue_item(&lru, &br->qi, br);
		mutex_release(&reclaim_lock);
		return 0;
	}

	uint64_t block = buffer_block(br);
	hash_delete(&br->bd->ctl->cache, &block, sizeof(block));
	mutex_release(&br->bd->ctl->cachelock);
	buffer_put(br);
	mutex_release(&reclaim_lock);
	return amount;
}

int dm_block_cache_insert(struct blockdev *bd, uint64_t block, struct buffer *buf, int flags)
{
	mutex_acquire(&bd->ctl->cachelock);

	struct buffer *prev = 0;
	buf->__block = block;
	block += buf->bd->partbegin;
	bool exist = hash_lookup(&bd->ctl->cache, &block, sizeof(block)) != NULL;

	if(exist && !(flags & BLOCK_CACHE_OVERWRITE)) {
		mutex_release(&bd->ctl->cachelock);
		return -EEXIST;
	}

	buffer_inc_refcount(buf);
	if(exist) {
		hash_delete(&bd->ctl->cache, &block, sizeof(block));
	}
	buf->trueblock = block;
	hash_insert(&bd->ctl->cache, &buf->trueblock, sizeof(buf->trueblock), &buf->hash_elem, buf);

	mutex_release(&bd->ctl->cachelock);
	queue_enqueue_item(&lru, &buf->qi, buf);
	if(exist) {
		/* overwritten, so will not writeback */
		queue_remove(&lru, &prev->qi);
		buffer_put(prev);
	}
	return 0;
}

struct buffer *dm_block_cache_get(struct blockdev *bd, uint64_t block)
{
	mutex_acquire(&bd->ctl->cachelock);
	block += bd->partbegin;

	struct buffer *e;
	if((e = hash_lookup(&bd->ctl->cache, &block, sizeof(block))) == NULL) {
		mutex_release(&bd->ctl->cachelock);
		return 0;
	}

	buffer_inc_refcount(e);
	mutex_release(&bd->ctl->cachelock);
	queue_remove(&lru, &e->qi);
	queue_enqueue_item(&lru, &e->qi, e);
	return e;
}

int block_cache_request(struct ioreq *req, off_t initial_offset, size_t total_bytecount, uint8_t *buffer)
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
		if(br->__block == block) {
			offset = initial_offset;
		}
		size_t copy_amount = req->bd->ctl->blocksize - offset;
		if(copy_amount > bytecount)
			copy_amount = bytecount;
		size_t position = (br->__block - block) * req->bd->ctl->blocksize;
		memcpy(buffer + position, br->data + offset, copy_amount);
		bytecount -= copy_amount;
		position += copy_amount;
		buffer_put(br);
	}
	return total_bytecount;
}

