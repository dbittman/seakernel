/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/string.h>
#include <sea/mm/kmalloc.h>

static void copy_pde(page_dir_t *pd, page_dir_t *parent_pd, int idx)
{
	if(!parent_pd[idx])
		return;
	page_table_t *parent = (addr_t *)((parent_pd[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	page_table_t table = mm_alloc_physical_page(), *entries;
	entries = (addr_t *)(table+PHYS_PAGE_MAP);
	int i;
	for(i=0;i<512;i++)
	{
		if(parent[i])
		{
			unsigned attr = parent[i] & ATTRIB_MASK;
			addr_t parent_page = parent[i] & PAGE_MASK;
			if(attr & PAGE_LINK) {
				/* we're just linking to the paren't physical page.
				 * see warnings about PAGE_LINK where PAGE_LINK is
				 * defined */
				entries[i] = parent_page | attr;
			} else {
				addr_t new_page = mm_alloc_physical_page();
				memcpy((void *)(new_page + PHYS_PAGE_MAP), (void *)(parent_page + PHYS_PAGE_MAP), PAGE_SIZE);
				entries[i] = new_page | attr;
			}
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
	page_dir_t pd = mm_alloc_physical_page();
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
	pdpt_t pdpt = mm_alloc_physical_page();
	memset((void *)(pdpt + PHYS_PAGE_MAP), 0, PAGE_SIZE);
	int i;
	for(i=0;i<512;i++)
		copy_pdpte((addr_t *)(pdpt+PHYS_PAGE_MAP), parent_pdpt, i);
	unsigned attr = parent_pml4[idx] & ATTRIB_MASK;
	pml4[idx] = pdpt | attr;
}

/* Accepts virtual, returns virtual */
void arch_mm_vm_clone(struct vmm_context *oldcontext, struct vmm_context *newcontext)
{
	addr_t pml4_phys;
	pml4_t *pml4 = kmalloc_ap(0x1000, &pml4_phys);
	pml4_t *parent_pml4 = (pml4_t *)oldcontext->root_virtual;
	
	if(pd_cur_data)
		mutex_acquire(&pd_cur_data->lock);
	unsigned int i;
	/* manually set up the pml4e #0 */
	pml4[0] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_USER | PAGE_WRITE;
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
	
	if(pd_cur_data)
		mutex_release(&pd_cur_data->lock);
	newcontext->root_virtual = (addr_t)pml4;
	newcontext->root_physical = pml4_phys;
	/* TODO: audit these locks */
	mutex_create(&newcontext->lock, MT_NOSCHED);
}

