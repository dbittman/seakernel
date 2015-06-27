#include <sea/tm/ticker.h>
#include <sea/mm/kmalloc.h>
#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/tm/async_call.h>

struct ticker *ticker_create(struct ticker *ticker, int flags)
{
	if(!ticker) {
		ticker = kmalloc(sizeof(struct ticker));
		ticker->flags = flags | TICKER_KMALLOC;
	} else {
		ticker->flags = flags;
	}
	ticker->tick = 0;
	heap_create(&ticker->heap, 0, HEAPMODE_MIN);
	return ticker;
}

void ticker_tick(struct ticker *ticker, uint64_t nanoseconds)
{
	ticker->tick += nanoseconds;
	uint64_t key;
	void *data;
	/* this is not exactly thread safe. But it doesn't matter!
	 * if something happens to work it's way in and replace the
	 * head element, then if the previous key was less than tick
	 * then it would be removed as well anyway. Which can happen
	 * on the next tick if need be.
	 */
	if(heap_peek(&ticker->heap, &key, &data) == 0) {
		if(key < ticker->tick) {
			/* get the data again, since it's cheap and
			 * we need to in case something bubbled up
			 * through the heap between the call to
			 * peak and now */
			heap_pop(&ticker->heap, &key, &data);
			/* handle the time-event */
			struct async_call *call = (struct async_call *)data;
			async_call_execute(call);
			async_call_destroy(call);
		}
	}
}

void ticker_insert(struct ticker *ticker, time_t nanoseconds, struct async_call *call)
{
	heap_insert(&ticker->heap, nanoseconds + ticker->tick, call);
}

void ticker_destroy(struct ticker *ticker)
{
	if(ticker->flags & TICKER_KMALLOC) {
		kfree(ticker);
	}
}

