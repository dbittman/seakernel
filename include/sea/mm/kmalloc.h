#ifndef __SEA_MM_KMALLOC_H
#define __SEA_MM_KMALLOC_H

#include <sea/types.h>

void slab_kfree(void *data);
void *slab_kmalloc(size_t size);
void slab_init(addr_t start, addr_t end);

#define kmalloc(a) slab_kmalloc(a)
#define kfree(a) slab_kfree(a)

#endif

