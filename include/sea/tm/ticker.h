#ifndef __SEA_TM_TICKER_H
#define __SEA_TM_TICKER_H

#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/mutex.h>

#define TICKER_KMALLOC 1

struct ticker {
	int flags;
	uint64_t tick;
	struct heap heap;
	mutex_t lock;
};

#endif

