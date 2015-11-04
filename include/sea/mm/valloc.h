#ifndef __SEA_MM_VALLOC_H
#define __SEA_MM_VALLOC_H

#include <sea/types.h>
#include <sea/mutex.h>

struct valloc_region {
	addr_t start;
	long npages;
	int flags;
};

#define VALLOC_MAGIC 0x87A110C8

struct valloc {
	long npages; /* number of psize pages in this region */
	addr_t start, end;
	long nindex; /* number of index pages */
	size_t psize; /* minimum allocation size, minimum PAGE_SIZE */
	struct mutex lock;
	int flags;
	/*at*/ long last;
	uint32_t magic;
};

#define VALLOC_ALLOC        1
struct valloc *valloc_create(struct valloc *va, addr_t start, addr_t end, size_t page_size,
		int flags);

void valloc_destroy(struct valloc *va);
struct valloc_region *valloc_allocate(struct valloc *va, struct valloc_region *reg,
		size_t np);
void valloc_reserve(struct valloc *va, struct valloc_region *reg);
void valloc_deallocate(struct valloc *va, struct valloc_region *reg);
int valloc_count_used(struct valloc *va);
struct valloc_region *valloc_split_region(struct valloc *va, struct valloc_region *reg,
		struct valloc_region *nr, size_t np);
#endif

