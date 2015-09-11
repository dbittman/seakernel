/* valloc.c: 2014
 * Allocates regions of virtual memory, of nearly arbitrary granularity
 * (must be multiples of PAGE_SIZE)
 *
 * current implementation is done as a bitmap with next-fit. This
 * isn't particularly good, or anything, but it's reasonably fast
 * and simple.
 *
 * WARNING: This is REALLY SLOW for large areas (>32 bits) of virtual
 * memory */

#include <sea/kernel.h>
#include <sea/mm/valloc.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>

/* helper macros.
 * TODO: Make a bitmap class */

#define SET_BIT(start,index) \
	(*((uint8_t *)start + (index / 8)) |= (1 << (index % 8)))

#define CLEAR_BIT(start,index) \
	(*((uint8_t *)start + (index / 8)) &= ~(1 << (index % 8)))

#define TEST_BIT(start,index) \
	(*((uint8_t *)start + (index / 8)) & (1 << (index % 8)))

static void __valloc_set_bits(struct valloc *va, long start, long count)
{
	for(long idx = start;idx < (start + count); idx++) {
		if(start >= va->nindex)
			assert(!TEST_BIT(va->start, idx));
		SET_BIT(va->start, idx);
	}
}

static int __valloc_count_bits(struct valloc *va)
{
	int count = 0;
	for(long idx = 0;idx < va->npages; idx++) {
		if(TEST_BIT(va->start, idx))
			++count;
	}
	return count;
}

static void __valloc_clear_bits(struct valloc *va, long start, long count)
{
	for(long idx = start;idx < (start + count); idx++) {
		if(start >= va->nindex)
			assert(TEST_BIT(va->start, idx));
		CLEAR_BIT(va->start, idx);
		assert(!TEST_BIT(va->start, idx));
	}
}
/* performs a linear next-fit search */
static long __valloc_get_start_index(struct valloc *va, long np)
{
	long start = -1;
	long count = 0;
	long idx = va->last;
	if(idx >= va->npages)
		idx=0;
	long prev = idx;
	do {
		int res = TEST_BIT(va->start, idx);
		if(start == -1 && res == 0) {
			/* we aren't checking a region for length, 
			 * and we found an empty bit. Start checking
			 * this region for length */
			start = idx;
			count=1;
			if(count == np)
				break;
		} else {
			/* we're checking a region for length. If this
			 * bit is 0, then we add to the count. Otherwise,
			 * we reset start to start looking for a new region */
			if(res == 0)
				count++;
			else {
				start = -1;
				count = 0;
			}
			/* if this region is long enough, break out */
			if(count == np)
				break;
		}
		idx++;
		/* need to wrap around due to next-fit */
		if(idx >= va->npages) {
			/* the region can't wrap around... */
			start = -1;
			count=0;
			idx=0;
		}
	} while(idx != prev);
	/* if it's a reasonable and legal region, return it */
	if(start != -1 && start < va->npages && count >= np) {
		/* and update 'last' for next-fit */
		va->last = start + np;
		return start;
	}
	return -1;
}

static void __valloc_populate_index(struct valloc *va, int flags)
{
	/* we can have larger pages than PAGE_SIZE, but here
	 * we need to map it in, and we need to work in increments
	 * of the system page size */
	int mm_pages = (va->nindex * va->psize) / PAGE_SIZE;
	for(int i=0;i<mm_pages;i++) {
		if(flags & VALLOC_USERMAP)
			user_map_if_not_mapped(va->start + i * PAGE_SIZE);
		else
			map_if_not_mapped(va->start + i * PAGE_SIZE);
	}
	__valloc_set_bits(va, 0, va->nindex);
}

static void __valloc_depopulate_index(struct valloc *va)
{
	int mm_pages = (va->nindex * va->psize) / PAGE_SIZE;
	for(int i=0;i<mm_pages;i++) {
		mm_vm_unmap(va->start + i*PAGE_SIZE, 0);
	}
}

struct valloc *valloc_create(struct valloc *va, addr_t start, addr_t end, size_t page_size,
		int flags)
{
	int leftover = (end - start) % page_size;
	end -= leftover;
	assert(!((end - start) % page_size));
	if(!va) {
		/* careful! kmalloc uses valloc... */
		va = (void *)kmalloc(sizeof(struct valloc));
		va->flags |= VALLOC_ALLOC;
	} else {
		memset(va, 0, sizeof(*va));
	}

	va->flags |= flags;
	va->start = start;
	va->end = end;
	va->psize = page_size;
	mutex_create(&va->lock, MT_NOSCHED);

	va->npages = (end - start) / page_size;
	va->nindex = ((va->npages-1) / (8 * page_size)) + 1;
	__valloc_populate_index(va, flags);
	return va;
}

void valloc_destroy(struct valloc *va)
{
	mutex_destroy(&va->lock);
	__valloc_depopulate_index(va);
	if(va->flags & VALLOC_ALLOC)
		kfree(va);
}

struct valloc_region *valloc_allocate(struct valloc *va, struct valloc_region *reg,
		size_t np)
{
	mutex_acquire(&va->lock);
	/* find and set the region */
	long index = __valloc_get_start_index(va, np);
	__valloc_set_bits(va, index, np);
	assert(index < va->npages);
	mutex_release(&va->lock);
	if(index == -1)
		return 0;
	if(!reg) {
		reg = (void *)kmalloc(sizeof(struct valloc_region));
		reg->flags = VALLOC_ALLOC;
	} else {
		reg->flags = 0;
	}
	reg->start = va->start + index * va->psize;
	reg->npages = np;
	return reg;
}

void valloc_reserve(struct valloc *va, struct valloc_region *reg)
{
	mutex_acquire(&va->lock);
	int start_index = (reg->start - va->start) / va->psize;
	assert(start_index+reg->npages <= va->npages && start_index >= va->nindex);
	__valloc_set_bits(va, start_index, reg->npages);
	mutex_release(&va->lock);
}

int valloc_count_used(struct valloc *va)
{
	mutex_acquire(&va->lock);
	int ret = __valloc_count_bits(va);
	mutex_release(&va->lock);
	return ret;
}

void valloc_deallocate(struct valloc *va, struct valloc_region *reg)
{
	mutex_acquire(&va->lock);
	int start_index = (reg->start - va->start) / va->psize;
	assert(start_index+reg->npages <= va->npages && start_index >= va->nindex);
	__valloc_clear_bits(va, start_index, reg->npages);
	va->last = start_index;
	mutex_release(&va->lock);
	if(reg->flags & VALLOC_ALLOC)
		kfree(reg);
}

struct valloc_region *valloc_split_region(struct valloc *va, struct valloc_region *reg,
		struct valloc_region *nr, size_t np)
{
	if(!nr) {
		nr = (void *)kmalloc(sizeof(struct valloc_region));
		nr->flags = VALLOC_ALLOC;
	} else {
		nr->flags = 0;
	}
	mutex_acquire(&va->lock);
	/* with the newly created valloc_region, we can fix up the
	 * data (resize and set the pointer for the new one).
	 * The bitmap doesn't need to be updated! */
	long old = reg->npages;
	reg->npages = np;
	long start_index_reg = (reg->start - va->start) / va->psize;
	long start_index_nr = start_index_reg + np;
	nr->start = va->start + start_index_nr * va->psize;
	nr->npages = old - np;
	mutex_release(&va->lock);
	return nr;
}

