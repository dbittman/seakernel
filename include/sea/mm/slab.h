#ifndef SLAB_H
#define SLAB_H
#include <sea/types.h>
#include <sea/mm/vmem.h>
#include <sea/config.h>
#include <sea/mm/valloc.h>
#define SLAB_MAGIC 0x11235813

enum transfer_vals {
	TO_EMPTY=0, 
	TO_PARTIAL=1, 
	TO_FULL=2
};

struct slab {
	unsigned magic;
	unsigned id;
	unsigned num_pages;
	volatile unsigned obj_used;/* Ref count */
	unsigned obj_num;
	/* For usage in the slab lists */
	struct slab *next, *prev;
	struct slab_cache *parent; /* Pointer to the slab cache that this slab is part of */
	volatile unsigned short *stack;
	unsigned short stack_arr[MAX_OBJ_ID];
	struct valloc_region vreg;
};

struct slab_cache {
	struct slab *empty, *partial, *full;
	/* Signed because an id of -1 means unused */
	short id;
	unsigned obj_size;
	unsigned slab_count;
};

#define NUM_SCACHES (PAGE_SIZE / sizeof(struct slab_cache))
#define OBJ_SIZE(s) (((struct slab_cache *)(s->parent))->obj_size)
#define FIRST_OBJ(s) ((addr_t)((addr_t)s+sizeof(struct slab)))

#endif

