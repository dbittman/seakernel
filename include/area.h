#ifndef AREA_H
#define AREA_H

#define NUM_NODES(v) ((((v->num_ipages*0x1000)/sizeof(vnode_t)) > MAX_NODES) ? MAX_NODES : ((v->num_ipages*0x1000)/sizeof(vnode_t)))
#include <mutex.h>
typedef struct vmem_node {
	unsigned addr;
	unsigned num_pages;
	struct vmem_node *next;
} vnode_t;

/* {[Index][...data...]}*/
typedef struct vmem_area {
	unsigned addr;
	short num_ipages;
	mutex_t lock;
	unsigned max;
	/* This is the last PHYSICAL node */
	vnode_t *first;
	unsigned char nodes[MAX_NODES];
	unsigned used_nodes;
} vma_t;

vnode_t *insert_vmem_area(vma_t *v, unsigned num_p);
int remove_vmem_area(vma_t *v, vnode_t *n);
int init_vmem_area(vma_t *v, unsigned addr, unsigned max, int num_ipages);
vnode_t *find_vmem_area(vma_t *v, unsigned addr);

#endif
