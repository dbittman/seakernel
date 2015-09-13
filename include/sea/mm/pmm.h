#ifndef __SEA_MM_PMM_H
#define __SEA_MM_PMM_H
#include <sea/mutex.h>
#include <sea/types.h>
#include <stdbool.h>

struct mm_physical_region {
	addr_t address;
	size_t size;
	addr_t alignment;
};

void arch_mm_physical_memset(void *addr, int c, size_t length);
addr_t mm_physical_allocate(size_t, bool);
void mm_physical_deallocate(addr_t address);

extern unsigned long pm_num_pages;

#endif
