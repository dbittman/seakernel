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
			mm_free_physical_page(tmp & PAGE_MASK);
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

