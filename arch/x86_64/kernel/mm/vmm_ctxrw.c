#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

bool arch_mm_context_write(struct vmm_context *ctx, addr_t address, void *src, size_t length)
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
		offset = address & (0x1000 - 1);

		if(offset + length > 0x1000)
			panic(0, "mm_context_write crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return false;
		}
		destp = ptv[ptidx] & PAGE_MASK_PHYSICAL;
	} else {
		offset = address & (0x200000 - 1);

		if(offset + length > 0x200000)
			panic(0, "mm_context_write crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		destp = pdv[pdidx] & PAGE_MASK_PHYSICAL;
	}
	memcpy((void *)(destp + PHYS_PAGE_MAP + offset), src, length);
	return true;
}

bool arch_mm_context_read(struct vmm_context *ctx, void *output, addr_t address, size_t length)
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
		offset = address & (0x1000 - 1);

		if(offset + length > 0x1000)
			panic(0, "mm_context_read crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK_PHYSICAL) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return false;
		}
		destp = ptv[ptidx] & PAGE_MASK_PHYSICAL;
	} else {
		offset = address & (0x200000 - 1);

		if(offset + length > 0x200000)
			panic(0, "mm_context_read crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		destp = pdv[pdidx] & PAGE_MASK_PHYSICAL;
	}
	memcpy(output, (void *)(destp + PHYS_PAGE_MAP + offset), length);
	return true;
}

