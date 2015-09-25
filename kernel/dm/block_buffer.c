#include <sea/dm/block.h>
#include <sea/errno.h>
#include <sea/tm/thread.h>
#include <sea/tm/kthread.h>
#include <sea/tm/timing.h>
#include <stdatomic.h>
struct llist dirty_list;
mutex_t dlock;
struct kthread syncer;

#if 0
void buffer_sync(struct buffer *buf);
int block_buffer_syncer(struct kthread *kt, void *arg)
{
	while(!kthread_is_joining(kt)) {
		mutex_acquire(&dlock);
		if(dirty_list.num > 0) {
			struct buffer *buf = ll_entry(struct buffer *, dirty_list.head);
			buffer_inc_refcount(buf);
			int count = buf->bd->wq.count;
			if(buf->flags & BUFFER_DIRTY)
				buffer_sync(buf);
			mutex_release(&dlock);
			buffer_put(buf);
			if(count > 0)
				tm_thread_delay(ONE_MILLISECOND * 10);
		} else {
			mutex_release(&dlock);
			tm_thread_delay(ONE_SECOND);
		}
	}
}
#endif
void block_buffer_init(void)
{
	mutex_create(&dlock, 0);
	ll_create_lockless(&dirty_list);
	//kthread_create(&syncer, "[ksync]", 0, block_buffer_syncer, 0);
}

void buffer_sync(struct buffer *buf)
{
	if(!(atomic_fetch_or(&buf->flags, BUFFER_WRITEPENDING) & BUFFER_WRITEPENDING)) {
		assert(buf->flags & BUFFER_DLIST);
		struct ioreq *req = ioreq_create(buf->bd, buf->dev, WRITE, buf->block, 1);
		atomic_fetch_add(&req->refs, 1);
		block_elevator_add_request(req);
		ioreq_put(req);
	}
}

/* TODO: one dlist per blockdevice? */
int buffer_sync_all_dirty(void)
{
	mutex_acquire(&dlock);
	struct llistnode *ln, *next;
	while(dirty_list.num > 0) {
		struct buffer *buf = ll_entry(struct buffer *, dirty_list.head);
		buffer_inc_refcount(buf);
		if(buf->flags & BUFFER_DIRTY)
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

struct buffer *buffer_create(blockdevice_t *bd, dev_t dev, uint64_t block, int flags, char *data)
{
	struct buffer *b = kmalloc(sizeof(struct buffer) + bd->blksz);
	b->bd = bd;
	b->block = block;
	b->flags = flags;
	b->refs = 1;
	b->dev = dev;
	memcpy(b->data, data, bd->blksz);
	return b;
}

void buffer_put(struct buffer *buf)
{
	assert(buf->refs > 0);
	if(atomic_fetch_sub(&buf->refs, 1) == 1) {
		assert(!(buf->flags & BUFFER_DIRTY) && !(buf->flags & BUFFER_DLIST));
		kfree(buf);
	} else {
		mutex_acquire(&dlock);
		if(buf->flags & BUFFER_DIRTY) {
			if(!(buf->flags & BUFFER_DLIST)) {
				/* changed to dirty, so add to list */
				ll_do_insert(&dirty_list, &buf->dlistnode, buf);
				atomic_fetch_or(&buf->flags, BUFFER_DLIST);
			}
		} else {
			if(buf->flags & BUFFER_DLIST) {
				ll_do_remove(&dirty_list, &buf->dlistnode, 0);
				atomic_fetch_and(&buf->flags, ~BUFFER_DLIST);
			}
		}
		mutex_release(&dlock);
	}
}

void buffer_inc_refcount(struct buffer *buf)
{
	assert(buf->refs > 0);
	atomic_fetch_add(&buf->refs, 1);
}

void block_elevator_add_request(struct ioreq *req)
{
	queue_enqueue_item(&req->bd->wq, &req->qi, req);
	tm_thread_poke(req->bd->elevator.thread);
}

int block_cache_get_bufferlist(struct llist *blist, struct ioreq *req)
{
	int ret = 0;
	size_t count = req->count;
	size_t block = req->block;
	while(count) {
		struct buffer *br = dm_block_cache_get(req->bd, block);
		if(!br) {
			atomic_fetch_add(&req->refs, 1);
			async_call_create(&current_thread->blockreq_call, 0, (void (*)(unsigned long))block_elevator_add_request,
					(unsigned long)req, ASYNC_CALL_PRIORITY_MEDIUM);
			tm_thread_block_schedule_work(&req->blocklist, THREADSTATE_UNINTERRUPTIBLE, &current_thread->blockreq_call);
			/* TODO: do this automatically? */
			struct workqueue *wq = current_thread->blockreq_call.queue;
			if(wq)
				workqueue_delete(wq, &current_thread->blockreq_call);

			assert(req->flags & IOREQ_COMPLETE);
			if(req->flags & IOREQ_FAILED) {
				return ret;
			}
			br = dm_block_cache_get(req->bd, block); // TODO: get this from elevator
			assert(br);
			atomic_fetch_and(&br->flags, ~BUFFER_LOCKED);
		}
		ll_do_insert(blist, &br->lnode, br);
		ret++;
		count--;
		block++;
	}
	return ret;
}

