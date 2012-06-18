/* Allocates sections of vmem. This allows us to allocate sections 
 * of any range of virtual memory, effectively allowing better 
 * management of those allocations. Copyright (c) 2010 Daniel Bittman. 
 * Written to allow easier implementation of slab allocation. 
 */
#include <kernel.h>
#include <memory.h>
#include <task.h>

vnode_t *insert_vmem_area(vma_t *v, unsigned num_p)
{
	assert(v && num_p);
	if((num_p * PAGE_SIZE + v->addr+(v->num_ipages*PAGE_SIZE)) > v->max)
		return 0;
	mutex_on(&v->lock);
	vnode_t *n = (vnode_t *)v->first;
	vnode_t *new=0;
	if(!n)
	{
		super_cli();
		assert(!v->nodes[0]);
		if(!n)
			n = (vnode_t *)v->addr;
		n->addr = v->addr + v->num_ipages*PAGE_SIZE;
		n->num_pages = num_p;
		n->next=0;
		v->nodes[0]=1;
		new=n;
		v->first = n;
	} else
	{
		while(n && (unsigned)n < (v->addr + v->num_ipages*PAGE_SIZE))
		{
			if((n->addr + n->num_pages*PAGE_SIZE) + num_p*PAGE_SIZE >= v->max) {
				mutex_off(&v->lock);
				return 0;
			}
			if(n->next)
			{
				if(n->next->addr <= n->addr)
					panic(PANIC_MEM | PANIC_NOSYNC, "vmem alloc index not sorted (virtual memory corrupted)!");
				if(((n->next->addr - 
						(n->addr + n->num_pages*PAGE_SIZE)) / PAGE_SIZE) >= num_p)
					break;
			} else
			{
				/* Do we have room for one more? */
				if((((unsigned)n + sizeof(vnode_t)) > 
						(v->addr+v->num_ipages*PAGE_SIZE))) {
					mutex_off(&v->lock);
					return 0;
				}
				break;
			}
			n=n->next;
		}
		if((unsigned)n >= (v->addr + v->num_ipages*PAGE_SIZE)) {
			mutex_off(&v->lock);
			return 0;
		}
		/* n is the node to insert after. */
		unsigned i=0;
		while(i < NUM_NODES(v) && v->nodes[i]) ++i;
		assert(i<NUM_NODES(v));
		v->nodes[i]=1;
		new = (vnode_t *)(v->addr + i*sizeof(vnode_t));
		memset(new, 0, sizeof(vnode_t));
		new->next = n->next;
		n->next = new;
		new->num_pages = num_p;
		new->addr = n->addr + n->num_pages * PAGE_SIZE;
		if(new->next)
			assert(new->num_pages * PAGE_SIZE + new->addr <= new->next->addr);
		assert(n->num_pages * PAGE_SIZE + n->addr <= n->next->addr);
	}
	v->used_nodes++;
	mutex_off(&v->lock);
	return new;
}

int remove_vmem_area(vma_t *v, vnode_t *n)
{
	mutex_on(&v->lock);
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
	v->used_nodes--;
	mutex_off(&v->lock);
	return 0;
}

vnode_t *find_vmem_area(vma_t *v, unsigned addr)
{
	if(!v)
		return 0;
	mutex_on(&v->lock);
	vnode_t *t = v->first;
	while(t) {
		if(addr >= t->addr && addr < (t->addr + t->num_pages*PAGE_SIZE))
			break;
		t=t->next;
	}
	mutex_off(&v->lock);
	return t;
}

int init_vmem_area(vma_t *v, unsigned addr, unsigned max, int num_ipages)
{
	memset(v, 0, sizeof(vma_t));
	v->addr = addr;
	v->num_ipages=num_ipages;
	v->max = max;
	v->used_nodes=0;
	create_mutex(&v->lock);
	return 0;
}
