#ifndef __SEA_TM_TICKER_H
#define __SEA_TM_TICKER_H

#include <sea/types.h>
#include <sea/lib/heap.h>
#include <sea/mutex.h>
#include <sea/tm/async_call.h>

#define TICKER_KMALLOC 1

struct ticker {
	int flags;
	uint64_t tick;
	struct heap heap;
	mutex_t lock;
};

struct ticker *ticker_create(struct ticker *ticker, int flags);
void ticker_tick(struct ticker *ticker, uint64_t nanoseconds);
void ticker_insert(struct ticker *ticker, time_t nanoseconds, struct async_call *call);
void ticker_destroy(struct ticker *ticker);
#endif

