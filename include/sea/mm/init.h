#ifndef __SEA_MM_INIT_H
#define __SEA_MM_INIT_H

#include <sea/boot/multiboot.h>
#include <types.h>
void mm_init(struct multiboot *m);
void kmalloc_create(char *name, unsigned (*init)(addr_t, addr_t), 
	addr_t (*alloc)(size_t, char), void (*free)(void *));

#endif
