#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
bool arch_mm_context_virtual_changeattr(struct vmm_context *ctx, addr_t virtual, int flags, size_t length)
{
	if(length != 0x200000 && length != 0x1000) {
		panic(0, "unsupported page size %x", length);
	}

	int pml4idx = PML4_INDEX(virtual);
	int pdptidx = PDPT_INDEX(virtual);
	int pdidx = PD_INDEX(virtual);
	
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		return false;
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return false;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	if(length == 0x1000) {
		int ptidx = PT_INDEX(virtual);
		if(!pdv[pdidx]) {
			return false;
		} else {
			if(pdv[pdidx] & PAGE_LARGE)
				return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
		addr_t old = atomic_load(&ptv[ptidx]);
		if(!old)
			return false;
		addr_t new = (old & PAGE_MASK_PHYSICAL) | flags;
		if(!atomic_compare_exchange_strong(&ptv[ptidx], &old, new))
			return false;
	} else {
		addr_t old = atomic_load(&pdv[pdidx]);
		if(!old)
			return false;
		addr_t new = (old & PAGE_MASK_PHYSICAL) | flags;
		if(!atomic_compare_exchange_strong(&pdv[pdidx], &old, new))
			return false;
	}
#if CONFIG_SMP
	x86_maybe_tlb_shootdown(virtual);
#endif
	return true;
}

bool arch_mm_virtual_changeattr(addr_t virtual, int flags, size_t length)
{
	struct vmm_context *ctx;
	if(current_process) {
		ctx = &current_process->vmm_context;
	} else {
		ctx = &kernel_context;
	}

	return arch_mm_context_virtual_changeattr(ctx, virtual, flags, length);
}

