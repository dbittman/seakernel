#include <sea/tm/ticker.h>
#include <sea/mm/kmalloc.h>
#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/tm/async_call.h>
#include <sea/mutex.h>
#include <sea/kernel.h>

struct ticker *ticker_create(struct ticker *ticker, int flags)
{
	if(!ticker) {
		ticker = kmalloc(sizeof(struct ticker));
		ticker->flags = flags | TICKER_KMALLOC;
	} else {
		ticker->flags = flags;
	}
	ticker->tick = 0;
	heap_create(&ticker->heap, HEAP_LOCKLESS, HEAPMODE_MIN);
	mutex_create(&ticker->lock, MT_NOSCHED);
	return ticker;
}

void ticker_tick(struct ticker *ticker, uint64_t nanoseconds)
{
	ticker->tick += nanoseconds;
	uint64_t key;
	void *data;
	if(heap_peek(&ticker->heap, &key, &data) == 0) {
		if(key < ticker->tick) {
			/* get the data again, since it's cheap and
			 * we need to in case something bubbled up
			 * through the heap between the call to
			 * peak and now */
			mutex_acquire(&ticker->lock);
			int res = heap_pop(&ticker->heap, &key, &data);
			mutex_release(&ticker->lock);
			if(res == 0) {
				/* handle the time-event */
				struct async_call *call = (struct async_call *)data;
				printk_safe(0, "-> %x\n", call);
				async_call_execute(call);
				async_call_destroy(call);
			}
		}
	}
}

void ticker_insert(struct ticker *ticker, time_t nanoseconds, struct async_call *call)
{
	kprintf("TICKER INSERT %x\n", call);
	assert(call);
	mutex_acquire(&ticker->lock);
	heap_insert(&ticker->heap, nanoseconds + ticker->tick, call);
	mutex_release(&ticker->lock);
}

void ticker_destroy(struct ticker *ticker)
{
	heap_destroy(&ticker->heap);
	mutex_destroy(&ticker->lock);
	if(ticker->flags & TICKER_KMALLOC) {
		kfree(ticker);
	}
}

