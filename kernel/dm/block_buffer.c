#include <sea/dm/block.h>
#include <sea/errno.h>
#include <sea/tm/thread.h>
struct llist dirty_list;
mutex_t dlock;

void block_buffer_init(void)
{
	mutex_create(&dlock, MT_NOSCHED);
	ll_create_lockless(&dirty_list);
}

void buffer_sync(struct buffer *buf)
{
	assert(buf->flags & BUFFER_DLIST);
	assert(buf->flags & BUFFER_DIRTY);
	panic(0, "NOT IMPLEMENTED");
	
	and_atomic(&buf->flags, ~BUFFER_DIRTY);
}

int buffer_sync_all_dirty(void)
{
	mutex_acquire(&dlock);
	struct llistnode *ln, *next;
	while(dirty_list.num > 0) {
		struct buffer *buf = ll_entry(struct buffer *, dirty_list.head);
		buffer_inc_refcount(buf);
		buffer_sync(buf);
		mutex_release(&dlock);
		buffer_put(buf);
		tm_schedule();
		switch(tm_thread_got_signal(current_thread)) {
			case SA_RESTART:
				return -ERESTART;
			case 0:
				break;
			default:
				return -EINTR;
		}
		mutex_acquire(&dlock);
	}
	mutex_release(&dlock);
	return 0;
}

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
		assert(!(buf->flags & BUFFER_DIRTY) && !(buf->flags & BUFFER_DLIST));
		kfree(buf);
	} else {
		mutex_acquire(&dlock);
		if(buf->flags & BUFFER_DIRTY) {
			if(!(buf->flags & BUFFER_DLIST)) {
				/* changed to dirty, so add to list */
				ll_do_insert(&dirty_list, &buf->dlistnode, buf);
				or_atomic(&buf->flags, BUFFER_DLIST);
			}
		} else {
			if(buf->flags & BUFFER_DLIST) {
				ll_remove(&dirty_list, &buf->dlistnode);
				and_atomic(&buf->flags, ~BUFFER_DLIST);
			}
		}
		mutex_release(&dlock);
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


