#ifndef AREA_H
#define AREA_H
#include <mutex.h>
#include <types.h>
#define NUM_NODES(v) ((((v->num_ipages*PAGE_SIZE)/sizeof(vnode_t)) > \
			MAX_NODES) ? MAX_NODES : ((v->num_ipages*PAGE_SIZE)/sizeof(vnode_t)))

typedef struct vmem_node {
	addr_t addr;
	unsigned num_pages;
	struct vmem_node *next;
} vnode_t;

/* {[Index][...data...]}*/
typedef struct vmem_area {
	addr_t addr;
	u16 num_ipages;
	mutex_t lock;
	addr_t max;
	/* This is the last PHYSICAL node */
	vnode_t *first;
	unsigned char nodes[MAX_NODES];
	unsigned used_nodes;
} vma_t;

vnode_t *insert_vmem_area(vma_t *v, unsigned num_p);
int remove_vmem_area(vma_t *v, vnode_t *n);
int init_vmem_area(vma_t *v, addr_t addr, addr_t max, int num_ipages);
vnode_t *find_vmem_area(vma_t *v, addr_t addr);

#endif
