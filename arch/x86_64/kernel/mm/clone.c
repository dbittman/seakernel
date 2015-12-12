/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/string.h>
#include <sea/mm/kmalloc.h>
#include <sea/syscall.h>

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
		}
	}
	
	newcontext->root_virtual = (addr_t)pml4;
	newcontext->root_physical = pml4_phys;
	spinlock_create(&newcontext->lock);
	mm_context_virtual_map(newcontext, MEMMAP_SYSGATE_ADDRESS, sysgate_page, PAGE_PRESENT | PAGE_USER, PAGE_SIZE);
}

