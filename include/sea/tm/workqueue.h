#ifndef __SEA_TM_WORKQUEUE_H
#define __SEA_TM_WORKQUEUE_H

#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/mutex.h>
#define WORKQUEUE_KMALLOC 1

struct workqueue {
	int flags;
	struct heap tasks;
	_Atomic int count;
	mutex_t lock;
};

struct workqueue *workqueue_create(struct workqueue *wq, int flags);
void workqueue_destroy(struct workqueue *wq);
void workqueue_insert(struct workqueue *wq, struct async_call *call);
int workqueue_delete(struct workqueue *wq, struct async_call *call);
int workqueue_dowork(struct workqueue *wq);

#endif

