#include <sea/dm/block.h>

struct buffer *buffer_create(blockdevice_t *bd, uint64_t block, int flags, char *data)
{
	struct buffer *b = kmalloc(sizeof(struct buffer) + bd->blksz);
	b->bd = bd;
	b->block = block;
	b->flags = flags;
	b->refs = 1;
	memcpy(b->data, data, bd->blksz);
	return b;
}

void buffer_put(struct buffer *buf)
{
	assert(buf->refs > 0);
	if(sub_atomic(&buf->refs, 1) == 0) {
		kfree(buf);
	}
}

void buffer_inc_refcount(struct buffer *buf)
{
	assert(buf->refs > 0);
	add_atomic(&buf->refs, 1);
}

static inline void __add_request(struct ioreq *req)
{
	add_atomic(&req->refs, 1);
	queue_enqueue_item(&req->bd->wq, &req->qi, req);
	tm_thread_set_state(req->bd->elevator.thread, THREADSTATE_RUNNING);
}

int block_cache_get_bufferlist(struct llist *blist, struct ioreq *req)
{
	int ret = 0;
	size_t count = req->count;
	size_t block = req->block;
	while(count) {
		struct buffer *br = dm_block_cache_get(req->bd, block);
		if(!br) {
			/* TODO: cleanup patterns like this */
			cpu_disable_preemption();
			tm_thread_add_to_blocklist(current_thread, &req->blocklist);
			mutex_acquire(&current_thread->block_mutex);
			__add_request(req);
			if(current_thread->blocklist)
				tm_thread_set_state(current_thread, THREADSTATE_UNINTERRUPTIBLE);
			mutex_release(&current_thread->block_mutex);
			cpu_enable_preemption();
			tm_schedule();
			assert(req->flags & IOREQ_COMPLETE);
			if(req->flags & IOREQ_FAILED) {
				return ret;
			}
			br = dm_block_cache_get(req->bd, block); // TODO: get this from elevator
		}
		ll_do_insert(blist, &br->lnode, br);
		ret++;
		count--;
		block++;
	}
	return ret;
}


