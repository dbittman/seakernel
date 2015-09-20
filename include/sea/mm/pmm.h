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
void mm_physical_memcpy(void *dest, void *src, size_t length, int);
void mm_physical_increment_count(addr_t page);
int mm_physical_decrement_count(addr_t page);

extern unsigned long pm_num_pages;

#define PHYS_MEMCPY_MODE_DEST 0
#define PHYS_MEMCPY_MODE_SRC  1
#define PHYS_MEMCPY_MODE_BOTH 2
#endif

