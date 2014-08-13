#include <sea/tm/process.h>
#include <sea/tm/context.h>

void arch_tm_set_current_task_marker(pml4_t *pml4, addr_t task)
{
	addr_t addr = CURRENT_TASK_POINTER, page, virt;
	pdpt_t *pdpt;
	page_dir_t *pd;
	page_table_t *pt;
	
	if(!pml4[PML4_IDX(addr/0x1000)])
		pml4[PML4_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pdpt = (addr_t *)((pml4[PML4_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[PDPT_IDX(addr/0x1000)])
		pdpt[PDPT_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pd = (addr_t *)((pdpt[PDPT_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[PAGE_DIR_IDX(addr/0x1000)])
		pd[PAGE_DIR_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	
	pt = (addr_t *)((pd[PAGE_DIR_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP);
		if(!pt[PAGE_TABLE_IDX(addr/0x1000)]) {
		page = arch_mm_alloc_physical_page_zero();
		virt = (page + PHYS_PAGE_MAP);
		
		pt[PAGE_TABLE_IDX(addr/0x1000)] = page | PAGE_PRESENT | PAGE_WRITE;
	} else
		virt = (pt[PAGE_TABLE_IDX(addr/0x1000)] & PAGE_MASK) + PHYS_PAGE_MAP;
	*(addr_t *)(virt) = task;
}

void arch_tm_set_kernel_stack(addr_t start, addr_t end)
{
	set_kernel_stack(current_tss, end);
}
