/* Functions for mapping of addresses */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <cpu-x86_64.h>
int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(kernel_task && !(opt & MAP_PDLOCKED))
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
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
	if(kernel_task) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
	#endif
	if(kernel_task && !(opt & MAP_PDLOCKED))
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
	
	return 0;
}
