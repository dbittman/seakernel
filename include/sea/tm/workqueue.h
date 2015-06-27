#ifndef __SEA_TM_WORKQUEUE_H
#define __SEA_TM_WORKQUEUE_H

#include <sea/types.h>
#include <sea/lib/heap.h>

struct workqueue {
	int flags;
	struct heap tasks;
	int count;
};

#endif
