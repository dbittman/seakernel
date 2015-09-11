/* Functions for mapping of addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/asm/system.h>
#include <sea/string.h>
int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(pd_cur_data && !(opt & MAP_PDLOCKED))
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((pd_cur_data) ? pd_cur_data->root_virtual : kernel_context.root_virtual);
	if(!pml4[vp4])
		pml4[vp4] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		pd[vdir] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	pt[vtbl] = (phys & PAGE_MASK) | attr;
	asm("invlpg (%0)"::"r" (virt));
	if(!(opt & MAP_NOCLEAR))
		memset((void *)(virt&PAGE_MASK), 0, 0x1000);
	#if CONFIG_SMP
	if(pd_cur_data) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
	#endif
	if(pd_cur_data && !(opt & MAP_PDLOCKED))
		mutex_release(&pd_cur_data->lock);
	return 0;
}

int arch_mm_vm_early_map(pml4_t *pml4, addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);

	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	addr_t x = arch_mm_alloc_physical_page_zero();
	if(!pml4[vp4])
		pml4[vp4] = x | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		pd[vdir] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	pt[vtbl] = (phys & PAGE_MASK) | attr;
	
	if(!(opt & MAP_NOCLEAR)) 
		memset((void *)(virt&PAGE_MASK), 0, 0x1000);
	
	return 0;
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
	
	/* TODO: make these all atomic? */
	addr_t *pml4v = (addr_t *)ctx->root_virtual;
	if(!pml4v[pml4idx]) {
		pml4v[pml4idx] = mm_physical_allocate(0x1000, true) | flags;
	}
	addr_t *pdptv = (addr_t *)((pml4v[pml4idx] & PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdptv[pdptidx]) {
		pdptv[pdptidx] = mm_physical_allocate(0x1000, true) | flags;
	}
	addr_t *pdv = (addr_t *)((pdptv[pdptidx] & PAGE_MASK) + PHYS_PAGE_MAP); //TODO PAGE_MASK SHOULD HANDLE NOEXEC?
	if(length == 0x1000) {
		int ptidx = PT_INDEX(virtual);
		if(!pdv[pdidx]) {
			pdv[pdidx] = mm_physical_allocate(0x1000, true) | flags;
		}
		addr_t *ptv = (addr_t *)((pdv[pdidx] & PAGE_MASK) + PHYS_PAGE_MAP);
		if(!ptv[ptidx]) {
			ptv[ptidx] = physical | flags;
			asm volatile("invlpg (%0)" :: "r"(virtual));
		} else {
			result = false;
		}
	} else {
		if(!pdv[pdidx]) {
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
		/* TODO: clean this up (make it's own function) */
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

bool arch_mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length)
{
	return arch_mm_context_virtual_map(&current_process->vmm_context, virtual,
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


