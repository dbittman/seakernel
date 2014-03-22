#ifndef __SEA_MM_PMM_H
#define __SEA_MM_PMM_H

#include <types.h>
void mm_copy_page_physical(addr_t src, addr_t dest);
void mm_zero_page_physical(addr_t page);
addr_t mm_alloc_physical_page();
void mm_free_physical_page(addr_t page);
void mm_pm_init();

#endif
