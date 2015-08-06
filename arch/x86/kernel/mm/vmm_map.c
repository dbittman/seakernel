/* Functions for mapping of addresses */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/mm/pmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/asm/system.h>
int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	unsigned vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vdir = PAGE_DIR_IDX(vpage);
	addr_t p;
	unsigned *pd = page_directory;
	if(pd_cur_data && !(opt & MAP_PDLOCKED))
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[vdir])
	{
		p = mm_alloc_physical_page();
		mm_zero_page_physical(p);
		pd[vdir] = p | PAGE_WRITE | PAGE_PRESENT | (attr & PAGE_USER);
		flush_pd();
	}
	page_tables[vpage] = (phys & PAGE_MASK) | attr;
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

