#include <sea/types.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>

#include <sea/tm/async_call.h>
#include <sea/tm/workqueue.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
/* TODO: roll this out to all kernel objects */
#define KOBJ_CREATE(obj,flags,alloc_flag) do {\
	if(!obj) {\
		obj = kmalloc(sizeof(*obj)); \
		obj->flags = flags | alloc_flag; \
	} else {\
		memset(obj, 0, sizeof(*obj)); \
		obj->flags = flags; \
	} \
	} while(0)

#define KOBJ_DESTROY(obj,alloc_flag) do {\
	if(obj->flags & alloc_flag)\
		kfree(obj);\
	} while(0)

struct workqueue *workqueue_create(struct workqueue *wq, int flags)
{
	KOBJ_CREATE(wq, flags, WORKQUEUE_KMALLOC);
	heap_create(&wq->tasks, HEAP_LOCKLESS, HEAPMODE_MAX);
	mutex_create(&wq->lock, MT_NOSCHED);
	return wq;
}

void workqueue_destroy(struct workqueue *wq)
{
	heap_destroy(&wq->tasks);
	mutex_destroy(&wq->lock);
	KOBJ_DESTROY(wq, WORKQUEUE_KMALLOC);
}

void workqueue_insert(struct workqueue *wq, struct async_call *call)
{
	/* Workqueues can be used by interrupts, so disable them while
	 * we operate on the data structures */
	if((addr_t)call >= KERNELMODE_STACKS_START && (addr_t)call < KERNELMODE_STACKS_END)
		panic(0, ">>>");
	int old = cpu_interrupt_set(0);
	mutex_acquire(&wq->lock);
	heap_insert(&wq->tasks, call->priority, call);
	call->queue = wq;
	mutex_release(&wq->lock);
	cpu_interrupt_set(old);
	atomic_fetch_add(&wq->count, 1);
}

int workqueue_delete(struct workqueue *wq, struct async_call *call)
{
	assert(call);
	int old = cpu_interrupt_set(0);
	mutex_acquire(&wq->lock);
	int r = heap_delete(&wq->tasks, call);
	call->queue = 0;
	mutex_release(&wq->lock);
	cpu_interrupt_set(old);
	return r;
}

int workqueue_dowork(struct workqueue *wq)
{
	struct async_call *call;
	int old = cpu_interrupt_set(0);
	mutex_acquire(&wq->lock);
	if(heap_pop(&wq->tasks, 0, (void **)&call) == 0) {
		call->queue = 0;
		mutex_release(&wq->lock);
		atomic_fetch_sub(&wq->count, 1);
		cpu_interrupt_set(old);
		/* handle async_call */
		async_call_execute(call);
		return 0;
	}
	mutex_release(&wq->lock);
	cpu_interrupt_set(old);
	return -1;
}

