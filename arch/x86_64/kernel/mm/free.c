/* free.c: Copyright (c) 2010 Daniel Bittman
 * Handles freeing an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
void free_pde(page_dir_t *pd, unsigned idx)
{
	if(!pd[idx]) 
		return;
	addr_t physical = pd[idx]&PAGE_MASK;
	page_table_t *table = (addr_t *)(physical + PHYS_PAGE_MAP);
	for(unsigned i=0;i<512;i++)
	{
		if(table[i])
		{
			addr_t tmp = table[i];
			table[i]=0;
			pm_free_page(tmp);
		}
	}
	pd[idx]=0;
	pm_free_page(physical);
}

void free_pdpte(pdpt_t *pdpt, unsigned idx)
{
	if(!pdpt[idx]) 
		return;
	addr_t physical = pdpt[idx]&PAGE_MASK;
	page_dir_t *pd = (addr_t *)(physical + PHYS_PAGE_MAP);
	for(unsigned i=0;i<512;i++)
		free_pde(pd, i);
	pdpt[idx]=0;
	pm_free_page(physical);
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
	pm_free_page(physical);
}

void free_thread_shared_directory()
{
	unsigned int S = PML4_IDX(START_FREE_LOCATION/0x1000);
	unsigned int E = PML4_IDX(TOP_TASK_MEM_EXEC/0x1000);
	for(unsigned i=S;i<E;i++)
		free_pml4e(current_task->pd, i);
}

/* free the pml4, not the entries */
void destroy_task_page_directory(task_t *p)
{
	if(p->flags & TF_LAST_PDIR) {
		addr_t *tmp;
		addr_t phys[4];
		phys[0] = p->pd[PML4_IDX(PDIR_DATA/0x1000)] & PAGE_MASK;
		tmp = (addr_t *)(phys[0] + PHYS_PAGE_MAP);
	
		phys[1] = tmp[PDPT_IDX(PDIR_DATA/0x1000)] & PAGE_MASK;
		tmp = (addr_t *)(phys[1] + PHYS_PAGE_MAP);

		phys[2] = tmp[PAGE_DIR_IDX(PDIR_DATA/0x1000)] & PAGE_MASK;
		tmp = (addr_t *)(phys[2] + PHYS_PAGE_MAP);

		phys[3] = tmp[PAGE_TABLE_IDX(PDIR_DATA/0x1000)] & PAGE_MASK;
		
		p->pd[PML4_IDX(PDIR_DATA/0x1000)]=0;
		
		for(int i=0;i<4;i++)
			pm_free_page(phys[i]);
	}
	
	addr_t *tmp;
	addr_t phys[4];
	phys[0] = p->pd[PML4_IDX(CURRENT_TASK_POINTER/0x1000)] & PAGE_MASK;
	tmp = (addr_t *)(phys[0] + PHYS_PAGE_MAP);

	phys[1] = tmp[PDPT_IDX(CURRENT_TASK_POINTER/0x1000)] & PAGE_MASK;
	tmp = (addr_t *)(phys[1] + PHYS_PAGE_MAP);

	phys[2] = tmp[PAGE_DIR_IDX(CURRENT_TASK_POINTER/0x1000)] & PAGE_MASK;
	tmp = (addr_t *)(phys[2] + PHYS_PAGE_MAP);

	phys[3] = tmp[PAGE_TABLE_IDX(CURRENT_TASK_POINTER/0x1000)] & PAGE_MASK;
	
	p->pd[PML4_IDX(CURRENT_TASK_POINTER/0x1000)]=0;
	
	for(int i=0;i<4;i++)
		pm_free_page(phys[i]);
	
	kfree(p->pd);
}

/* basically frees the stack */
void free_thread_specific_directory()
{
	unsigned int S = PML4_IDX(TOP_TASK_MEM_EXEC/0x1000);
	unsigned int E = PML4_IDX(TOP_TASK_MEM/0x1000);
	for(unsigned i=S;i<E;i++)
		free_pml4e(current_task->pd, i);
}
