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
	spinlock_create(&ticker->lock);
	return ticker;
}

void ticker_tick(struct ticker *ticker, uint64_t microseconds)
{
	ticker->tick += microseconds;
	cpu_disable_preemption();
	if(__current_cpu->preempt_disable > 1) {
		cpu_enable_preemption();
		return;
	}
	uint64_t key;
	void *data;
	if(heap_peek(&ticker->heap, &key, &data) == 0) {
		if(key < ticker->tick) {
			/* get the data again, since it's cheap and
			 * we need to in case something bubbled up
			 * through the heap between the call to
			 * peak and now */
			int old = cpu_interrupt_set(0);
			spinlock_acquire(&ticker->lock);
			int res = heap_pop(&ticker->heap, &key, &data);
			spinlock_release(&ticker->lock);
			cpu_interrupt_set(old);
			if(res == 0) {
				/* handle the time-event */
				struct async_call *call = (struct async_call *)data;
				call->queue = 0;
				async_call_execute(call);
			}
		}
	}
	cpu_enable_preemption();
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
	if(ticker->flags & TICKER_KMALLOC) {
		kfree(ticker);
	}
}

