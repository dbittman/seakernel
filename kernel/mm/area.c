/* Allocates sections of vmem. This allows us to allocate sections 
 * of any range of virtual memory, effectively allowing better 
 * management of those allocations. Copyright (c) 2010 Daniel Bittman. 
 * Written to allow easier implementation of slab allocation. 
 */
#include <sea/kernel.h>
#include <sea/mm/vmem.h>
#include <sea/tm/process.h>

#define NUM_NODES(v) ((((v->num_ipages*PAGE_SIZE)/sizeof(vnode_t)) > \
			MAX_NODES) ? MAX_NODES : ((v->num_ipages*PAGE_SIZE)/sizeof(vnode_t)))

static vnode_t *get_node_insert_location(vma_t *v, unsigned num_p)
{
	vnode_t *n = (vnode_t *)v->first;
	while(n && (addr_t)n < (v->addr + v->num_ipages*PAGE_SIZE))
	{
		if((n->addr + n->num_pages*PAGE_SIZE) + num_p*PAGE_SIZE >= v->max)
			return 0;
		if(n->next)
		{
			if(n->next->addr <= n->addr)
				panic(PANIC_MEM | PANIC_NOSYNC, "vmem alloc index not sorted "
												"(virtual memory corrupted)!");
			if(((n->next->addr - 
					(n->addr + n->num_pages*PAGE_SIZE)) / PAGE_SIZE) >= num_p)
				break;
		} else
		{
			/* Do we have room for one more? */
			if((((addr_t)n + sizeof(vnode_t)) > 
					(v->addr+v->num_ipages*PAGE_SIZE)))
				return 0;
			break;
		}
		n=n->next;
	}
	if((addr_t)n >= (v->addr + v->num_ipages*PAGE_SIZE))
		return 0;
	return n;
}

vnode_t *vmem_insert_node(vma_t *v, unsigned num_p)
{
	assert(v && num_p);
	if((num_p * PAGE_SIZE + v->addr+(v->num_ipages*PAGE_SIZE)) > v->max)
		return 0;
	mutex_acquire(&v->lock);
	vnode_t *n = (vnode_t *)v->first;
	vnode_t *newn=0;
	if(!n)
	{
		assert(!v->nodes[0]);
		n = (vnode_t *)v->addr;
		n->addr = v->addr + v->num_ipages*PAGE_SIZE;
		n->num_pages = num_p;
		n->next=0;
		v->nodes[0]=1;
		newn=n;
		v->first = n;
	} else
	{
		n = get_node_insert_location(v, num_p);
		if(!n) 
			goto out;
		/* n is the node to insert after. */
		unsigned i=0;
		while(i < NUM_NODES(v) && v->nodes[i]) ++i;
		assert(i<NUM_NODES(v));
		v->nodes[i]=1;
		newn = (vnode_t *)(v->addr + i*sizeof(vnode_t));
		memset(newn, 0, sizeof(vnode_t));
		newn->next = n->next;
		n->next = newn;
		newn->num_pages = num_p;
		newn->addr = n->addr + n->num_pages * PAGE_SIZE;
		if(newn->next)
			assert(newn->num_pages * PAGE_SIZE + newn->addr <= newn->next->addr);
		assert(n->num_pages * PAGE_SIZE + n->addr <= n->next->addr);
	}
	v->used_nodes++;
	out:
	mutex_release(&v->lock);
	return newn;
}

vnode_t *vmem_split_node(vma_t *v, vnode_t *n, unsigned new_np)
{
	mutex_acquire(&v->lock);
	unsigned i=0;
	assert(new_np && new_np < n->num_pages);
	while(i < NUM_NODES(v) && v->nodes[i]) ++i;
	/* TODO: User space can make this crash... */
	assert(i < NUM_NODES(v));
	v->nodes[i] = 1;
	vnode_t *newn = (vnode_t *)(v->addr + i * sizeof(vnode_t));
	memset(newn, 0, sizeof(*newn));
	newn->next = n->next;
	n->next = newn;

	newn->num_pages = (n->num_pages - new_np);
	n->num_pages = new_np;
	newn->addr = n->addr + n->num_pages*PAGE_SIZE;

	v->used_nodes++;
	mutex_release(&v->lock);
	return newn;
}

int vmem_remove_node(vma_t *v, vnode_t *n)
{
	mutex_acquire(&v->lock);
	if(n == v->first)
		v->first = n->next;
	else
	{
		vnode_t *t = v->first;
		while(t && t->next != n) t=t->next;
		assert(t);
		t->next = n->next;
	}
	n->next=0;
	unsigned i = (unsigned)(((addr_t)n - v->addr) / sizeof(vnode_t));
	assert((addr_t)n >= (addr_t)v->addr);
	assert((addr_t)n < ((addr_t)v->addr + v->num_ipages*PAGE_SIZE));
	assert(i < NUM_NODES(v));
	v->nodes[i]=0;
	v->used_nodes--;
	mutex_release(&v->lock);
	return 0;
}

vnode_t *vmem_find_node(vma_t *v, addr_t addr)
{
	if(!v)
		return 0;
	mutex_acquire(&v->lock);
	vnode_t *t = v->first;
	while(t) {
		if(addr >= t->addr && addr < (t->addr + t->num_pages*PAGE_SIZE))
			break;
		t=t->next;
	}
	mutex_release(&v->lock);
	return t;
}

int vmem_create(vma_t *v, addr_t addr, addr_t max, int num_ipages)
{
	memset(v, 0, sizeof(vma_t));
	v->addr = addr;
	v->num_ipages=num_ipages;
	v->max = max;
	v->used_nodes=0;
	int i;
	for(i=0;i<num_ipages;i++)
		map_if_not_mapped(addr + i * PAGE_SIZE);
	mutex_create(&v->lock, 0);
	return 0;
}

int vmem_create_user(vma_t *v, addr_t addr, addr_t max, int num_ipages)
{
	memset(v, 0, sizeof(vma_t));
	v->addr = addr;
	v->num_ipages=num_ipages;
	v->max = max;
	v->used_nodes=0;
	int i;
	for(i=0;i<num_ipages;i++)
		user_map_if_not_mapped(addr + i * PAGE_SIZE);
	mutex_create(&v->lock, 0);
	return 0;
}

