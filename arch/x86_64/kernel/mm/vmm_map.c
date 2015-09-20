/* Functions for mapping of addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/asm/system.h>
#include <sea/string.h>

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
	
	/* TODO: make these all atomic? */
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		pml4v[pml4idx] = mm_physical_allocate(0x1000, true) | flags | PAGE_WRITE; /* TODO: how to set these flags? If a mapping
																					 changes attr, then the parent flags should
																					 change attrs too? */
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		pdptv[pdptidx] = mm_physical_allocate(0x1000, true) | flags | PAGE_WRITE;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(length == 0x1000) {
		int ptidx = PT_INDEX(virtual);
		if(!pdv[pdidx]) {
			pdv[pdidx] = mm_physical_allocate(0x1000, true) | flags | PAGE_WRITE;
		} else {
			if(pdv[pdidx] & PAGE_LARGE)
				result = false;
		}
		if(result) {
			addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
			if(!ptv[ptidx]) {
				ptv[ptidx] = physical | flags;
				mm_physical_increment_count(physical);
				asm volatile("invlpg (%0)" :: "r"(virtual));
			} else {
				result = false;
			}
		}
	} else {
		if(!pdv[pdidx]) {
			mm_physical_increment_count(physical);
			pdv[pdidx] = physical | flags | PAGE_LARGE;
			asm volatile("invlpg (%0)" :: "r"(virtual));
		} else {
			result = false;
		}
	}
	if(clear)
		memset((void *)(physical + PHYS_PAGE_MAP), 0, length);
#if CONFIG_SMP
	if(result && pd_cur_data) {
		/* TODO: clean this up (make it's own function) (and figure out when to actually do this) */
		if(IS_KERN_MEM(virtual))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
					0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virtual)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
					0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif

	return result;
}

bool arch_mm_context_virtual_changeattr(struct vmm_context *ctx, addr_t virtual, int flags, size_t length)
{
	if(length != 0x200000 && length != 0x1000) {
		panic(0, "unsupported page size %x", length);
	}

	int pml4idx = PML4_INDEX(virtual);
	int pdptidx = PDPT_INDEX(virtual);
	int pdidx = PD_INDEX(virtual);
	
	/* TODO: make these all atomic? */
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		return false;
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return false;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(length == 0x1000) {
		int ptidx = PT_INDEX(virtual);
		if(!pdv[pdidx]) {
			return false;
		} else {
			if(pdv[pdidx] & PAGE_LARGE)
				return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
		addr_t old = atomic_load(&ptv[ptidx]);
		if(!old)
			return false;
		addr_t new = (old & PAGE_MASK) | flags;
		if(!atomic_compare_exchange_strong(&ptv[ptidx], &old, new))
			return false;
	} else {
		addr_t old = atomic_load(&pdv[pdidx]);
		if(!old)
			return false;
		addr_t new = (old & PAGE_MASK) | flags;
		if(!atomic_compare_exchange_strong(&pdv[pdidx], &old, new))
			return false;
	}
#if CONFIG_SMP
	if(pd_cur_data) {
		/* TODO: clean this up (make it's own function) (and figure out when to actually do this) */
		if(IS_KERN_MEM(virtual))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
					0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virtual)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS,
					0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif

	return true;
}

bool arch_mm_virtual_changeattr(addr_t virtual, int flags, size_t length)
{
	struct vmm_context *ctx;
	if(pd_cur_data) {
		ctx = pd_cur_data;
	} else {
		ctx = &kernel_context;
	}

	return arch_mm_context_virtual_changeattr(ctx, virtual, flags, length);
}

bool arch_mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length)
{
	struct vmm_context *ctx;
	if(pd_cur_data) {
		ctx = pd_cur_data;
	} else {
		ctx = &kernel_context;
	}

	return arch_mm_context_virtual_map(ctx, virtual,
			physical, flags, length);
}

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
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return false;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(!(pdv[pdidx] & PAGE_LARGE)) {
		int ptidx = PT_INDEX(address);
		offset = address & (0x1000 - 1);

		if(offset + length > 0x1000)
			panic(0, "mm_context_write crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return false;
		}
		destp = ptv[ptidx] & PAGE_MASK; //TODO: physical page mask (noexec bit?)
	} else {
		offset = address & (0x200000 - 1);

		if(offset + length > 0x200000)
			panic(0, "mm_context_write crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		destp = pdv[pdidx] & PAGE_MASK; //TODO: different masks for diff page sizes?
	}
	memcpy((void *)(destp + PHYS_PAGE_MAP + offset), src, length);
	return true;
}

void arch_mm_physical_memset(void *addr, int c, size_t length)
{
	addr_t start = (addr_t)addr + PHYS_PAGE_MAP;
	memset((void *)start, c, length);
}

void arch_mm_physical_memcpy(void *dest, void *src, size_t length, int mode)
{
	addr_t startd = (addr_t)dest;
	if(mode == PHYS_MEMCPY_MODE_DEST || mode == PHYS_MEMCPY_MODE_BOTH)
		startd += PHYS_PAGE_MAP;
	addr_t starts = (addr_t)src;
	if(mode == PHYS_MEMCPY_MODE_SRC || mode == PHYS_MEMCPY_MODE_BOTH)
		starts += PHYS_PAGE_MAP;
	memcpy((void *)startd, (void *)starts, length);
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
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return false;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(!(pdv[pdidx] & PAGE_LARGE)) {
		int ptidx = PT_INDEX(address);
		offset = address & (0x1000 - 1);

		if(offset + length > 0x1000)
			panic(0, "mm_context_read crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return false;
		}
		destp = ptv[ptidx] & PAGE_MASK; //TODO: physical page mask (noexec bit?)
	} else {
		offset = address & (0x200000 - 1);

		if(offset + length > 0x200000)
			panic(0, "mm_context_read crossed page boundary");

		if(!pdv[pdidx]) {
			return false;
		}
		destp = pdv[pdidx] & PAGE_MASK; //TODO: different masks for diff page sizes?
	}
	memcpy(output, (void *)(destp + PHYS_PAGE_MAP + offset), length);
	return true;
}


