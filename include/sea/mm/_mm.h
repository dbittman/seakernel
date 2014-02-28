#ifndef _SEA_MM__MM_H
#define _SEA_MM__MM_H

#include <sea/subsystem.h>

#if SUBSYSTEM != _SUBSYSTEM_MM
#error "_mm.h included from a non-mm source file"
#endif

#include <types.h>

unsigned __mm_slab_init(addr_t start, addr_t end);
void __mm_do_kfree_slab(void *ptr);
addr_t __mm_do_kmalloc_slab(size_t sz, char align);


#endif
