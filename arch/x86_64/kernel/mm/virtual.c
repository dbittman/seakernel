/* Defines functions for virtual memory management */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>

#include <sea/cpu/interrupt.h>
#include <sea/boot/init.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/loader/symbol.h>

extern void *boot_pml4;
alignas(0x1000) static uint64_t physmap_pages [513][512];
void arch_mm_virtual_init(struct vmm_context *context)
{
 	context->root_physical = (addr_t)&boot_pml4;
 	context->root_virtual = kernel_context.root_physical + MEMMAP_KERNEL_START;
 	mutex_create(&kernel_context.lock, MT_NOSCHED);
 	/* map in all physical memory */
 	pml4_t *pml4 = (pml4_t *)context->root_virtual;
 	addr_t address = 0;
 	memset(physmap_pages, 0, sizeof(physmap_pages));
	pdpt_t *pdpt = (pdpt_t *)((addr_t)physmap_pages[0]);
	for(int i=0;i<512;i++) {
		pdpt[i] = ((addr_t)physmap_pages[i+1] - MEMMAP_KERNEL_START) | PAGE_PRESENT | PAGE_WRITE;
		page_dir_t *pd = (page_dir_t *)((pdpt[i] & PAGE_MASK) + MEMMAP_KERNEL_START);
		for(int j=0;j<512;j++) {
			pd[j] = address | PAGE_PRESENT | PAGE_WRITE | (1 << 7);
			address += 0x200000;
		}
	}
 	pml4[PML4_INDEX(PHYS_PAGE_MAP)] = ((addr_t)pdpt - MEMMAP_KERNEL_START) | PAGE_PRESENT | PAGE_WRITE;
	/* Enable paging */
	set_ksf(KSF_PAGING);
}

