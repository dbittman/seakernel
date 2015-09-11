/* Functions for unmapping addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/asm/system.h>

addr_t arch_mm_context_virtual_unmap(struct vmm_context *ctx, addr_t address)
{
	int pml4idx = PML4_INDEX(address);
	int pdptidx = PDPT_INDEX(address);
	int pdidx = PD_INDEX(address);

	addr_t destp, offset;
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		return 0;
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		return 0;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(!(pdv[pdidx] & PAGE_LARGE)) {
		int ptidx = PT_INDEX(address);

		if(!pdv[pdidx]) {
			return 0;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			return 0;
		}
		destp = ptv[ptidx] & PAGE_MASK;
		ptv[ptidx] = 0;
	} else {
		if(!pdv[pdidx]) {
			return 0;
		}
		destp = pdv[pdidx] & PAGE_MASK; //TODO: different masks for diff page sizes?
		pdv[pdidx] = 0;
	}
	asm volatile("invlpg (%0)" :: "r"(address));
#if CONFIG_SMP
	if(pd_cur_data) {
		if(IS_KERN_MEM(address))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(address)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	return destp;
}

addr_t arch_mm_virtual_unmap(addr_t address)
{
	return arch_mm_context_virtual_unmap(&current_process->vmm_context, address);
}

