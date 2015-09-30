#include <sea/types.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>
#include <sea/tm/async_call.h>
#include <sea/tm/workqueue.h>
#include <sea/cpu/processor.h>
#include <sea/tm/thread.h>
#include <sea/cpu/interrupt.h>
#include <sea/kobj.h>

struct workqueue *workqueue_create(struct workqueue *wq, int flags)
{
	KOBJ_CREATE(wq, flags, WORKQUEUE_KMALLOC);
	heap_create(&wq->tasks, HEAP_LOCKLESS, HEAPMODE_MAX);
	spinlock_create(&wq->lock);
	return wq;
}

void workqueue_destroy(struct workqueue *wq)
{
	heap_destroy(&wq->tasks);
	spinlock_destroy(&wq->lock);
	KOBJ_DESTROY(wq, WORKQUEUE_KMALLOC);
}

void workqueue_insert(struct workqueue *wq, struct async_call *call)
{
	/* Workqueues can be used by interrupts, so disable them while
	 * we operate on the data structures */
	int old = cpu_interrupt_set(0);
	spinlock_acquire(&wq->lock);
	heap_insert(&wq->tasks, call->priority, call);
	call->queue = wq;
	spinlock_release(&wq->lock);
	cpu_interrupt_set(old);
	atomic_fetch_add(&wq->count, 1);
}

int workqueue_delete(struct workqueue *wq, struct async_call *call)
{
	assert(call);
	int old = cpu_interrupt_set(0);
	spinlock_acquire(&wq->lock);
	int r = heap_delete(&wq->tasks, call);
	call->queue = 0;
	spinlock_release(&wq->lock);
	cpu_interrupt_set(old);
	return r;
}

/* not allowed to do work if it could cause a deadlock.
 * See, the async_calls called from this function are supposed
 * to run in normal kernel context (not interrupt, etc). So,
 * it won't do any work if:
 *  - preempt is disabled
 *  - any locks are held
 */
int workqueue_dowork(struct workqueue *wq)
{
	int old = cpu_interrupt_set(0);
	assert(__current_cpu->preempt_disable == 0);
	if(current_thread->held_locks) {
		cpu_interrupt_set(old);
		return -1;
	}
	struct async_call *call;
	spinlock_acquire(&wq->lock);
	if(heap_pop(&wq->tasks, 0, (void **)&call) == 0) {
		call->queue = 0;
		spinlock_release(&wq->lock);
		atomic_fetch_sub(&wq->count, 1);
		cpu_interrupt_set(old);
		/* handle async_call */
		async_call_execute(call);
		return 0;
	}
	spinlock_release(&wq->lock);
	cpu_interrupt_set(old);
	return -1;
}

