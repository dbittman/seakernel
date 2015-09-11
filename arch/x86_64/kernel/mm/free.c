/* free.c: Copyright (c) 2010 Daniel Bittman
 * Handles freeing an address space */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/mm/kmalloc.h>
void free_pde(page_dir_t *pd, unsigned idx)
{
	if(!pd[idx]) 
		return;
	addr_t physical = pd[idx]&PAGE_MASK;
	assert(!(pd[idx] & (1 << 7)));
	page_table_t *table = (addr_t *)(physical + PHYS_PAGE_MAP);
	for(unsigned i=0;i<512;i++)
	{
		if(table[i])
		{
			addr_t tmp = table[i];
			table[i]=0;
			mm_free_physical_page(tmp & PAGE_MASK_PHYSICAL);
		}
	}
	pd[idx]=0;
	mm_free_physical_page(physical);
}

void free_pdpte(pdpt_t *pdpt, unsigned idx)
{
	if(!pdpt[idx]) 
		return;
	addr_t physical = pdpt[idx]&PAGE_MASK;
	assert(!(pdpt[idx] & (1 << 7)));
	page_dir_t *pd = (addr_t *)(physical + PHYS_PAGE_MAP);
	for(unsigned i=0;i<512;i++)
		free_pde(pd, i);
	pdpt[idx]=0;
	mm_free_physical_page(physical);
}

void free_pml4e(pml4_t *pml4, unsigned idx)
{
	if(!pml4[idx]) 
		return;
	addr_t physical = pml4[idx]&PAGE_MASK;
	pdpt_t *pdpt = (addr_t *)(physical + PHYS_PAGE_MAP);
	for(unsigned i=0;i<512;i++)
		free_pdpte(pdpt, i);
	pml4[idx]=0;
	mm_free_physical_page(physical);
}

void arch_mm_free_self_directory()
{
	unsigned int S = 0;
	unsigned int E = PML4_IDX(TOP_TASK_MEM_EXEC/0x1000);
	pml4_t *pml4 = (pml4_t *)current_process->vmm_context.root_virtual;
	for(unsigned i=S;i<E;i++)
		free_pml4e(pml4, i);
}

/* free the pml4, not the entries */
void arch_mm_destroy_directory(struct vmm_context *vc)
{
	pml4_t *pml4 = (pml4_t *)vc->root_virtual;
	unsigned int E = PML4_IDX(MEMMAP_KERNEL_START/0x1000);
	for(unsigned i=0;i<E;i++)
		free_pml4e(pml4, i);
	mm_physical_deallocate(vc->root_physical);
}

