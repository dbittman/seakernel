#ifndef __SEA_MM_PMM_H
#define __SEA_MM_PMM_H
#include <sea/mutex.h>
#include <sea/types.h>

struct mm_physical_region {
	addr_t address;
	size_t size;
	addr_t alignment;
};

void mm_copy_page_physical(addr_t src, addr_t dest);
void mm_zero_page_physical(addr_t page);
addr_t mm_alloc_physical_page();
void mm_free_physical_page(addr_t page);
void mm_pm_init();

void mm_pmm_init_contiguous(addr_t start);
void mm_pmm_register_contiguous_memory(addr_t region_start);
void mm_free_contiguous_region(struct mm_physical_region *p);
void mm_alloc_contiguous_region(struct mm_physical_region *p);

extern volatile addr_t pm_location;
extern volatile unsigned long pm_num_pages, pm_used_pages;
extern volatile uint64_t highest_page;
extern volatile uint64_t lowest_page;
extern int memory_has_been_mapped;
extern volatile addr_t placement;
extern mutex_t pm_mutex;

#endif
