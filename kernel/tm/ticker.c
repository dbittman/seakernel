#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/kernel.h>
#include <sea/lib/heap.h>
#include <sea/mm/kmalloc.h>
#include <sea/spinlock.h>
#include <sea/tm/async_call.h>
#include <sea/tm/ticker.h>
#include <sea/types.h>
#include <sea/tm/thread.h>
#include <sea/kobj.h>

struct ticker *ticker_create(struct ticker *ticker, int flags)
{
	KOBJ_CREATE(ticker, flags, TICKER_KMALLOC);
	ticker->tick = 0;
	heap_create(&ticker->heap, HEAP_LOCKLESS, HEAPMODE_MIN);
	spinlock_create(&ticker->lock);
	return ticker;
}

void ticker_tick(struct ticker *ticker, uint64_t microseconds)
{
	ticker->tick += microseconds;
	uint64_t key;
	void *data;
	if(heap_peek(&ticker->heap, &key, &data) == 0) {
		if(key < ticker->tick) {
			tm_thread_raise_flag(current_thread, THREAD_TICKER_DOWORK);
		}
	}
}

void ticker_dowork(struct ticker *ticker)
{
	uint64_t key;
	void *data;
	int old = cpu_interrupt_set(0);
	assert(!current_thread->blocklist);
	if(__current_cpu->preempt_disable > 0) {
		cpu_interrupt_set(old);
		return;
	}
	if(current_thread->held_locks) {
		cpu_interrupt_set(old);
		return;
	}
	while(heap_peek(&ticker->heap, &key, &data) == 0) {
		if(key < ticker->tick) {
			/* get the data again, since it's cheap and
			 * we need to in case something bubbled up
			 * through the heap between the call to
			 * peak and now */
			spinlock_acquire(&ticker->lock);
			int res = heap_pop(&ticker->heap, &key, &data);
			if(!res)
				tm_thread_lower_flag(current_thread, THREAD_TICKER_DOWORK);
			spinlock_release(&ticker->lock);
			if(res == 0) {
				/* handle the time-event */
				struct async_call *call = (struct async_call *)data;
				call->queue = 0;
				async_call_execute(call);
			}
		} else {
			break;
		}
	}
	cpu_interrupt_set(old);
}

void ticker_insert(struct ticker *ticker, time_t microseconds, struct async_call *call)
{
	assert(call);
	int old = cpu_interrupt_set(0);
	spinlock_acquire(&ticker->lock);
	heap_insert(&ticker->heap, microseconds + ticker->tick, call);
	call->queue = ticker;
	spinlock_release(&ticker->lock);
	cpu_interrupt_set(old);
}

int ticker_delete(struct ticker *ticker, struct async_call *call)
{
	int old = cpu_interrupt_set(0);
	spinlock_acquire(&ticker->lock);
	int r = heap_delete(&ticker->heap, call);
	call->queue = 0;
	spinlock_release(&ticker->lock);
	cpu_interrupt_set(old);
	return r;
}

void ticker_destroy(struct ticker *ticker)
{
	heap_destroy(&ticker->heap);
	spinlock_destroy(&ticker->lock);
	KOBJ_DESTROY(ticker, TICKER_KMALLOC);
}

