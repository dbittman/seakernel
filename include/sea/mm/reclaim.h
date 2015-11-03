#ifndef __SEA_MM_RECLAIM
#define __SEA_MM_RECLAIM

#include <sea/types.h>
#include <sea/lib/linkedlist.h>
struct reclaimer {
	size_t size;
	size_t (*fn)(void);
	struct linkedentry node;
};

void mm_reclaim_init(void);
void mm_reclaim_register(size_t (*fn)(void), size_t size);
size_t mm_reclaim_size(size_t size);
void mm_reclaim(void);

#endif

