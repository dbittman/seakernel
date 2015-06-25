#ifndef __SEA_TM_TICKER_H
#define __SEA_TM_TICKER_H

#include <sea/types.h>

#define TICKER_KMALLOC 1

struct ticker {
	int flags;
	uint64_t tick;
};

#endif

