#ifndef __SEA_MM_VMEM_H
#define __SEA_MM_VMEM_H

#include <sea/types.h>
#include <sea/mutex.h>

#define MAX_NODES 4096*4

typedef struct vmem_node {
	addr_t addr;
	unsigned num_pages;
	struct vmem_node *next;
} vnode_t;

/* {[Index][...data...]}*/
typedef struct vmem_area {
	addr_t addr;
	unsigned num_ipages;
	mutex_t lock;
	addr_t max;
	/* This is the last PHYSICAL node */
	vnode_t *first;
	unsigned char nodes[MAX_NODES];
	unsigned used_nodes;
} vma_t;

int vmem_create(vma_t *v, addr_t addr, addr_t max, int num_ipages);
vnode_t *vmem_find_node(vma_t *v, addr_t addr);
int vmem_remove_node(vma_t *v, vnode_t *n);
vnode_t *vmem_insert_node(vma_t *v, unsigned num_p);
vnode_t *vmem_split_node(vma_t *v, vnode_t *n, unsigned new_np);

#endif
