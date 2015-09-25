/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/string.h>
#include <sea/mm/kmalloc.h>
#include <sea/syscall.h>
static void copy_pde_large(page_dir_t *pd, page_dir_t *parent_pd, int idx, bool cow)
{
	addr_t parent = parent_pd[idx];
	if(parent & PAGE_LINK) {
		pd[idx] = parent;
	} else {
		addr_t new = mm_physical_allocate(0x200000, false);
		memcpy((void *)(new + PHYS_PAGE_MAP), (void *)((parent&PAGE_MASK) + PHYS_PAGE_MAP), 0x200000);
		pd[idx] = new | (parent & ATTRIB_MASK);
	}
}

static void copy_pde(page_dir_t *pd, page_dir_t *parent_pd, int idx, bool cow)
{
	if(!parent_pd[idx])
		return;
	if((addr_t)parent_pd[idx] & PAGE_LARGE) {
		return copy_pde_large(pd, parent_pd, idx, cow);
	}
	addr_t *parent = (addr_t *)((parent_pd[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	page_table_t table = mm_physical_allocate(0x1000, false), *entries;
	entries = (addr_t *)(table+PHYS_PAGE_MAP);
	int i;
	for(i=0;i<512;i++)
	{
		if(parent[i])
		{
			unsigned attr = parent[i] & ATTRIB_MASK;
			addr_t parent_page = parent[i] & PAGE_MASK;
			if((attr & PAGE_LINK)) {
				/* we're just linking to the paren't physical page.
				 * see warnings about PAGE_LINK where PAGE_LINK is
				 * defined */
				entries[i] = parent_page | attr;
			} else {
				addr_t new_page = mm_physical_allocate(0x1000, false);
				memcpy((void *)(new_page + PHYS_PAGE_MAP), (void *)(parent_page + PHYS_PAGE_MAP), PAGE_SIZE);
				entries[i] = new_page | attr;
			}
		} else
			entries[i]=0;
	}
	unsigned attr = parent_pd[idx] & ATTRIB_MASK;
	pd[idx] = table | attr;
}

void copy_pdpte(pdpt_t *pdpt, pdpt_t *parent_pdpt, int idx, bool cow)
{
	if(!parent_pdpt[idx])
		return;
	page_dir_t *parent_pd = (addr_t *)((parent_pdpt[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	page_dir_t pd = mm_physical_allocate(0x1000, false);
	memset((void *)(pd + PHYS_PAGE_MAP), 0, PAGE_SIZE);
	int i;
	for(i=0;i<512;i++)
		copy_pde((addr_t *)(pd+PHYS_PAGE_MAP), parent_pd, i, cow);
	unsigned attr = parent_pdpt[idx] & ATTRIB_MASK;
	pdpt[idx] = pd | attr;
}

void copy_pml4e(pml4_t *pml4, pml4_t *parent_pml4, int idx, bool cow)
{
	if(!parent_pml4[idx])
		return;
	pdpt_t *parent_pdpt = (addr_t *)((parent_pml4[idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	pdpt_t pdpt = mm_physical_allocate(0x1000, false);
	memset((void *)(pdpt + PHYS_PAGE_MAP), 0, PAGE_SIZE);
	int i;
	for(i=0;i<512;i++)
		copy_pdpte((addr_t *)(pdpt+PHYS_PAGE_MAP), parent_pdpt, i, cow);
	unsigned attr = parent_pml4[idx] & ATTRIB_MASK;
	pml4[idx] = pdpt | attr;
}

/* Accepts virtual, returns virtual */
void arch_mm_context_clone(struct vmm_context *oldcontext, struct vmm_context *newcontext)
{
	addr_t pml4_phys = mm_physical_allocate(0x1000, true);
	pml4_t *pml4 = (void *)(pml4_phys + PHYS_PAGE_MAP);
	pml4_t *parent_pml4 = (pml4_t *)oldcontext->root_virtual;
	
	pml4[511] = parent_pml4[511];
	for(unsigned int i=0;i<512;i++) {
		if(i >= PML4_INDEX(MEMMAP_KERNEL_START)) {
			pml4[i] = parent_pml4[i];
		} else if(i >= PML4_INDEX(MEMMAP_USERSPACE_MAXIMUM) || !current_process->pid) {
			copy_pml4e(pml4, parent_pml4, i, false);
		} else if(parent_pml4[i]) {
			copy_pml4e(pml4, parent_pml4, i, true);
		}
	}
	
	newcontext->root_virtual = (addr_t)pml4;
	newcontext->root_physical = pml4_phys;
	spinlock_create(&newcontext->lock);
}

