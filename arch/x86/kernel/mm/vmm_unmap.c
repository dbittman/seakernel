/* Functions for unmapping addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/mm/swap.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/asm/system.h>
/* send the TBL shootdown to cpus if:
 *  - tasking is enabled
 *  - either:
 * 	  - this is kernel memory
 *    - OR this is shared threading memory, and there is another thread
 *  - BUT NOT if we are unmapping the pd_cur_data page
 */

int arch_mm_vm_unmap_only(addr_t virt, unsigned locked)
{
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_tables[(virt&PAGE_MASK)/0x1000] = 0;
	asm("invlpg (%0)"::"r" (virt));
#if CONFIG_SMP
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_release(&pd_cur_data->lock);
	return 0;
}

int arch_mm_vm_unmap(addr_t virt, unsigned locked)
{
	/* This gives the virtual address of the table needed, and sets
	 * the correct place as zero */
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_acquire(&pd_cur_data->lock);
	addr_t p = page_tables[(virt&PAGE_MASK)/0x1000];
	page_tables[(virt&PAGE_MASK)/0x1000] = 0;
	asm("invlpg (%0)"::"r" (virt));
#if CONFIG_SMP
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	/* TODO: seriously, get rid of these pc->i_t references */
	if(pd_cur_data && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_release(&pd_cur_data->lock);
	if(p && !(p & PAGE_COW))
		mm_free_physical_page(p & PAGE_MASK);
	return 0;
}
