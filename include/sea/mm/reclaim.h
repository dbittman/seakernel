#ifndef __SEA_MM_RECLAIM
#define __SEA_MM_RECLAIM

#include <sea/types.h>

struct rec {
	size_t size;
	size_t (*fn)(void);
}

void mm_reclaim_init(void);
void mm_reclaim_register(size_t (*fn)(void), size_t size);
void mm_reclaim_size(size_t size);
void mm_reclaim(void);

#endif

