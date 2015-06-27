#include <sea/types.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/async_call.h>
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
	heap_create(&wq->tasks, 0);
}

void workqueue_destroy(struct workqueue *wq)
{
	KOBJ_DESTROY(wq, WORKQUEUE_KMALLOC);
}

void workqueue_insert(struct workqueue *wq, struct async_call *call)
{
	heap_insert(&wq->tasks, call->priority, call);
	add_atomic(&wq->count, 1);
}

int workqueue_dowork(struct workqueue *wq)
{
	struct async_call *call;
	if(heap_pop(&wq->tasks, 0, (void **)&call) == 0) {
		sub_atomic(&wq->count, 1);
		/* handle async_call */
		async_call_execute(call);
		async_call_destroy(call);
		return 0;
	}
	return -1;
}

