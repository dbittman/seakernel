#include <sea/dm/block.h>
#include <sea/dm/blockdev.h>
#include <sea/tm/blocking.h>
#include <sea/errno.h>
#include <sea/tm/thread.h>
#include <sea/tm/kthread.h>
#include <sea/tm/timing.h>
#include <stdatomic.h>
#include <sea/vsprintf.h>
struct linkedlist dirty_list;
struct mutex dlock;
struct kthread syncer;

void block_buffer_init(void)
{
	mutex_create(&dlock, 0);
	linkedlist_create(&dirty_list, LINKEDLIST_LOCKLESS);
	//kthread_create(&syncer, "[ksync]", 0, block_buffer_syncer, 0);
}

void buffer_sync(struct buffer *buf)
{
	if(!(atomic_fetch_or(&buf->flags, BUFFER_WRITEPENDING) & BUFFER_WRITEPENDING)) {
		assert(buf->flags & BUFFER_DLIST);
		struct ioreq *req = ioreq_create(buf->bd, WRITE, buf->__block, 1);
		atomic_fetch_add(&req->refs, 1);
		block_elevator_add_request(req);
		ioreq_put(req);
	}
}

int buffer_sync_all_dirty(void)
{
	printk(0, "[block]: syncing block buffers (%d buffers)\n", dirty_list.count);
	mutex_acquire(&dlock);
	while(dirty_list.count > 0) {
		struct buffer *buf = linkedlist_head(&dirty_list);
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

struct buffer *buffer_create(struct blockdev *bd, uint64_t block, int flags, unsigned char *data)
{
	struct buffer *b = kmalloc(sizeof(struct buffer) + bd->ctl->blocksize);
	b->bd = bd;
	b->__block = block;
	b->flags = flags;
	b->refs = 1;
	memcpy(b->data, data, bd->ctl->blocksize);
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
				linkedlist_insert(&dirty_list, &buf->dlistnode, buf);
				atomic_fetch_or(&buf->flags, BUFFER_DLIST);
			}
		} else {
			if(buf->flags & BUFFER_DLIST) {
				linkedlist_remove(&dirty_list, &buf->dlistnode);
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

bool block_elevator_add_request(void *data)
{
	struct ioreq *req = data;
	while(!mpscq_enqueue(&req->bd->ctl->queue, req))
		cpu_pause();
	tm_thread_poke(req->bd->ctl->elevator.thread);
	return true;
}

struct buffer *block_cache_get_first_buffer(struct ioreq *req)
{
	struct buffer *br = dm_block_cache_get(req->bd, req->block);
	if(!br) {
		atomic_fetch_add(&req->refs, 1);
		tm_thread_block_confirm(&req->blocklist, THREADSTATE_UNINTERRUPTIBLE,
				block_elevator_add_request, req);

		assert(req->flags & IOREQ_COMPLETE);
		if(req->flags & IOREQ_FAILED) {
			return NULL;
		}
		br = dm_block_cache_get(req->bd, req->block);
		assert(br);
		atomic_fetch_and(&br->flags, ~BUFFER_LOCKED);
	}
	return br;
}

