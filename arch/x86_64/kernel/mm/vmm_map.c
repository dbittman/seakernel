/* Functions for mapping of addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/asm/system.h>
#include <sea/string.h>

#if CONFIG_SMP
void x86_maybe_tlb_shootdown(addr_t virtual)
{
	/* TODO: when do actually send? */
	if(IS_KERN_MEM(virtual))
		x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
				0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	//else if((IS_THREAD_SHARED_MEM(virtual)))
	//	x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
	//			0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
}
#endif

static void __maybe_add_entry(addr_t *entries, int index, int flags)
{
	if(!entries[index]) {
		addr_t page = mm_physical_allocate(0x1000, true) | flags;
		addr_t expect = NULL;
		if(!atomic_compare_exchange_strong(&entries[index], &expect, page)) {
			mm_physical_deallocate(page & PAGE_MASK_PHYSICAL);
		}
	}
}

bool arch_mm_context_virtual_map(struct vmm_context *ctx, addr_t virtual,
		addr_t physical, int flags, size_t length)
{
	bool clear = flags & MAP_ZERO;
	flags &= ATTRIB_MASK;
	if(length != 0x200000 && length != 0x1000) {
		panic(0, "unsupported page size %x", length);
	}
	bool result = true;
	int pml4idx = PML4_INDEX(virtual);
	int pdptidx = PDPT_INDEX(virtual);
	int pdidx = PD_INDEX(virtual);
	
	addr_t expect = NULL;
	addr_t value = physical | flags;

	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	__maybe_add_entry(pml4v, pml4idx, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	__maybe_add_entry(pdptv, pdptidx, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	if(length == 0x1000) {
		int ptidx = PT_INDEX(virtual);
		__maybe_add_entry(pdv, pdidx, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
		if(pdv[pdidx] & PAGE_LARGE)
			result = false;
		if(result) {
			addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
			if(atomic_compare_exchange_strong(&ptv[ptidx], &expect, value)) {
				asm volatile("invlpg (%0)" :: "r"(virtual));
			} else {
				result = false;
			}
		}
	} else {
		value |= PAGE_LARGE;
		if(atomic_compare_exchange_strong(&pdv[pdidx], &expect, value)) {
			asm volatile("invlpg (%0)" :: "r"(virtual));
		} else {
			result = false;
		}
	}
	if(clear)
		memset((void *)(physical + PHYS_PAGE_MAP), 0, length);
#if CONFIG_SMP
	if(result) {
		x86_maybe_tlb_shootdown(virtual);
	}
#endif

	return result;
}

bool arch_mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length)
{
	struct vmm_context *ctx;
	if(current_process) {
		ctx = &current_process->vmm_context;
	} else {
		ctx = &kernel_context;
	}

	return arch_mm_context_virtual_map(ctx, virtual,
			physical, flags, length);
}


