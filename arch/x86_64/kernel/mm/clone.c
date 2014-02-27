/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <swap.h>
#include <cpu.h>

void copy_pde(page_dir_t *pd, page_dir_t *parent_pd, int idx)
{
	if(!parent_pd[idx])
		return;
	page_table_t *parent = (addr_t *)((parent_pd[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	page_table_t table = pm_alloc_page(), *entries;
	entries = (addr_t *)(table+PHYS_PAGE_MAP);
	int i;
	for(i=0;i<512;i++)
	{
		if(parent[i])
		{
			unsigned attr = parent[i] & ATTRIB_MASK;
			addr_t new_page = pm_alloc_page();
			addr_t parent_page = parent[i] & PAGE_MASK;
			memcpy((void *)(new_page + PHYS_PAGE_MAP), (void *)(parent_page + PHYS_PAGE_MAP), PAGE_SIZE);
			entries[i] = new_page | attr;
		} else
			entries[i]=0;
	}
	unsigned attr = parent_pd[idx] & ATTRIB_MASK;
	pd[idx] = table | attr;
}

void copy_pdpte(pdpt_t *pdpt, pdpt_t *parent_pdpt, int idx)
{
	if(!parent_pdpt[idx])
		return;
	page_dir_t *parent_pd = (addr_t *)((parent_pdpt[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	page_dir_t pd = pm_alloc_page();
	memset((void *)(pd + PHYS_PAGE_MAP), 0, PAGE_SIZE);
	int i;
	for(i=0;i<512;i++)
		copy_pde((addr_t *)(pd+PHYS_PAGE_MAP), parent_pd, i);
	unsigned attr = parent_pdpt[idx] & ATTRIB_MASK;
	pdpt[idx] = pd | attr;
}

void copy_pml4e(pml4_t *pml4, pml4_t *parent_pml4, int idx)
{
	if(!parent_pml4[idx])
		return;
	pdpt_t *parent_pdpt = (addr_t *)((parent_pml4[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	pdpt_t pdpt = pm_alloc_page();
	memset((void *)(pdpt + PHYS_PAGE_MAP), 0, PAGE_SIZE);
	int i;
	for(i=0;i<512;i++)
		copy_pdpte((addr_t *)(pdpt+PHYS_PAGE_MAP), parent_pdpt, i);
	unsigned attr = parent_pml4[idx] & ATTRIB_MASK;
	pml4[idx] = pdpt | attr;
}

/* Accepts virtual, returns virtual */
pml4_t *vm_clone(pml4_t *parent_pml4, char cow)
{
#if CONFIG_SWAP
	if(current_task && current_task->num_swapped)
		swap_in_all_the_pages(current_task);
#endif
	addr_t pml4_phys;
	pml4_t *pml4 = kmalloc_ap(0x1000, &pml4_phys);
	
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	unsigned int i;
	/* manually set up the pml4e #0 */
	pml4[0] = pm_alloc_page_zero() | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
	pdpt_t *pdpt = (addr_t *)((pml4[0] & PAGE_MASK) + PHYS_PAGE_MAP);
	memset(pdpt, 0, 0x1000);
	pdpt_t *parent_pdpt = (addr_t *)((parent_pml4[0] & PAGE_MASK) + PHYS_PAGE_MAP);
	pdpt[0] = parent_pdpt[0];
	for(i=1;i<512;i++)
		copy_pdpte(pdpt, parent_pdpt, i);
	
	for(i=1;i<512;i++)
	{
		if(i >= PML4_IDX(BOTTOM_HIGHER_KERNEL/0x1000) || parent_pml4[i] == 0)
			pml4[i] = parent_pml4[i];
		else
			copy_pml4e(pml4, parent_pml4, i);
	}
	pml4[PML4_IDX(PHYSICAL_PML4_INDEX/0x1000)] = pml4_phys;
	
	/* get the physical address of the page_dir_info for the new task, which is automatically
	 * copied in the copy loop above */
	addr_t info_phys;
	addr_t *tmp = (addr_t *)((pml4[PML4_IDX(PDIR_DATA/0x1000)] & PAGE_MASK)+PHYS_PAGE_MAP);
	tmp = (addr_t *)((tmp[PDPT_IDX(PDIR_DATA/0x1000)] & PAGE_MASK)+PHYS_PAGE_MAP);
	tmp = (addr_t *)((tmp[PAGE_DIR_IDX(PDIR_DATA/0x1000)] & PAGE_MASK)+PHYS_PAGE_MAP);
	
	info_phys = tmp[PAGE_TABLE_IDX(PDIR_DATA/0x1000)] & PAGE_MASK;
	
	struct pd_data *info = (struct pd_data *)(info_phys + PHYS_PAGE_MAP);
	memset(info, 0, 0x1000);
	info->count=1;
	/* create the lock. We assume that the only time that two threads
	 * may be trying to access this lock at the same time is when they're
	 * running on different processors, thus we get away with NOSCHED. Also, 
	 * calling tm_schedule() may be problematic inside code that is locked by
	 * this, but it may not be an issue. We'll see. */
	mutex_create(&info->lock, MT_NOSCHED);
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return pml4;
}

/* this sets up a new page directory in almost exactly the same way:
 * the directory is specific to the thread, but everything inside
 * the directory is just linked to the parent directory. The count
 * on the directory usage is increased, and the accounting page is 
 * linked so it can be accessed by both threads */
pml4_t *vm_copy(pml4_t *parent_pml4)
{
#if CONFIG_SWAP
	if(current_task && current_task->num_swapped)
		swap_in_all_the_pages(current_task);
#endif
	addr_t pml4_phys;
	pml4_t *pml4 = kmalloc_ap(0x1000, &pml4_phys);
	
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	pd_cur_data->count++;
	unsigned int i;
	for(i=0;i<512;i++)
	{
		if(parent_pml4[i] == 0 || i >= PML4_IDX(BOTTOM_HIGHER_KERNEL/0x1000) || i < PML4_IDX(TOP_TASK_MEM_EXEC/0x1000) || i == PML4_IDX(PDIR_DATA/0x1000))
		{
			pml4[i] = parent_pml4[i];
		} else {
			copy_pml4e(pml4, parent_pml4, i);
		}
	}
	pml4[PML4_IDX(PHYSICAL_PML4_INDEX/0x1000)] = pml4_phys;
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return pml4;
}
