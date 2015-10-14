#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

bool arch_mm_context_virtual_getmap(struct vmm_context *ctx, addr_t address, addr_t *phys, int *flags)
{
	int pml4idx = PML4_INDEX(address);
	int pdptidx = PDPT_INDEX(address);
	int pdidx = PD_INDEX(address);

	addr_t destp, offset;
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		return false;
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return false;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
	if(!(pdv[pdidx] & PAGE_LARGE)) {
		int ptidx = PT_INDEX(address);

		if(!pdv[pdidx]) {
			return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return false;
		}
		destp = ptv[ptidx];
	} else {
		if(!pdv[pdidx]) {
			return false;
		}
		destp = pdv[pdidx];
	}
	if(phys)
		*phys = destp & PAGE_MASK_PHYSICAL;
	if(flags)
		*flags = destp & ATTRIB_MASK;
	return true;
}

bool arch_mm_virtual_getmap(addr_t address, addr_t *phys, int *flags)
{
	struct vmm_context *ctx;
	if(current_process) {
		ctx = &current_process->vmm_context;
	} else {
		ctx = &kernel_context;
	}

	return arch_mm_context_virtual_getmap(ctx, address, phys, flags);
}

