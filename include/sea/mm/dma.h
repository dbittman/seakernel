#ifndef __SEA_MM_DMA_H
#define __SEA_MM_DMA_H

#include <sea/types.h>
int mm_allocate_dma_buffer(size_t length, addr_t *, addr_t *physical);
int arch_mm_allocate_dma_buffer(size_t length, addr_t *, addr_t *physical);

#endif
