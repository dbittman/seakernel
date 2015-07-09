#ifndef __SEA_TM_WORKQUEUE_H
#define __SEA_TM_WORKQUEUE_H

#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/mutex.h>
#define WORKQUEUE_KMALLOC 1

struct workqueue {
	int flags;
	struct heap tasks;
	int count;
	mutex_t lock;
};

#endif
