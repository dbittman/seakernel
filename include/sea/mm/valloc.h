#ifndef __SEA_MM_VALLOC_H
#define __SEA_MM_VALLOC_H

#include <sea/types.h>
#include <sea/mutex.h>

struct valloc_region {
	addr_t start;
	int npages;
	int flags;
};

struct valloc {
	int npages; /* number of psize pages in this region */
	addr_t start, end;
	int nindex; /* number of index pages */
	size_t psize; /* minimum allocation size, minimum PAGE_SIZE */
	mutex_t lock;
	int flags, last;
};

#define VALLOC_ALLOC 1

struct valloc *valloc_create(struct valloc *va, addr_t start, addr_t end, size_t page_size,
		int flags);

void valloc_destroy(struct valloc *va);
struct valloc_region *valloc_allocate(struct valloc *va, struct valloc_region *reg,
		size_t np);
void valloc_deallocate(struct valloc *va, struct valloc_region *reg);
struct valloc_region *valloc_split_region(struct valloc *va, struct valloc_region *reg,
		struct valloc_region *nr, int np);
#endif

