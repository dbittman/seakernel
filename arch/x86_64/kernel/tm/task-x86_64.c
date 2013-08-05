#include <kernel.h>
#include <task.h>

void arch_specific_set_current_task(pml4_t *pml4, addr_t task)
{
	addr_t addr = CURRENT_TASK_POINTER;
	pdpt_t *pdpt;
	page_dir_t *pd;
	page_table_t *pt;
	
	if(!pml4[PML4_IDX(addr/0x1000)])
		pml4[PML4_IDX(addr/0x1000)] = pm_alloc_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pdpt = (addr_t *)((pml4[PML4_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[PDPT_IDX(addr/0x1000)])
		pdpt[PDPT_IDX(addr/0x1000)] = pm_alloc_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pd = (addr_t *)((pdpt[PDPT_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[PAGE_DIR_IDX(addr/0x1000)])
		pd[PAGE_DIR_IDX(addr/0x1000)] = pm_alloc_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pt = (addr_t *)((pd[PAGE_DIR_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);

	addr_t page = pm_alloc_page_zero();
	addr_t virt = (page + PHYS_PAGE_MAP);
	
	pt[PAGE_TABLE_IDX(addr/0x1000)] = page | PAGE_PRESENT | PAGE_WRITE;
	*(addr_t *)(virt) = task;
}
