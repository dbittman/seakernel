#ifndef __SEA_MM_DMA_H
#define __SEA_MM_DMA_H

#include <sea/types.h>
#include <sea/mm/pmm.h>

struct dma_region {
	struct mm_physical_region p;
	addr_t v;
};

int mm_allocate_dma_buffer(struct dma_region *);
int mm_free_dma_buffer(struct dma_region *);

#endif

