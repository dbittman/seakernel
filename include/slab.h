#ifndef SLAB_H
#define SLAB_H
#include <area.h>
#include <config.h>
#define SLAB_MAGIC 0x11235813
#define S_ALIGN 1

extern addr_t slab_start, slab_end;
extern vma_t slab_area_alloc;

enum transfer_vals {
	TO_EMPTY=0, 
	TO_PARTIAL=1, 
	TO_FULL=2
};

typedef struct slab_header_s {
	unsigned magic;
	unsigned id;
	unsigned short num_pages;
	unsigned short flags;
	unsigned short obj_used;/* Ref count */
	unsigned short obj_num;
	/* For usage in the slab lists */
	struct slab_header_s *next, *prev;
	addr_t parent; /* Pointer to the slab cache that this slab is part of */
	unsigned short *stack;
	unsigned short stack_arr[MAX_OBJ_ID];
	vnode_t *vnode;
} slab_t;

typedef struct slab_cache_s {
	slab_t *empty, *partial, *full;
	/* Signed because an id of -1 means unused */
	short id;
	unsigned short flags;
	unsigned obj_size;
	unsigned slab_count;
} slab_cache_t;

#define NUM_SCACHES (PAGE_SIZE / sizeof(slab_cache_t))
#define OBJ_SIZE(s) (((slab_cache_t *)(s->parent))->obj_size)
#define FIRST_OBJ(s) ((addr_t)((s->flags & S_ALIGN) ? ((addr_t)s+PAGE_SIZE) \
	: ((addr_t)s+sizeof(slab_t))))

#endif

