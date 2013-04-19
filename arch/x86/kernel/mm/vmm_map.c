/* Functions for mapping of addresses */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
int vm_map(unsigned virt, unsigned phys, unsigned attr, unsigned opt)
{
	unsigned vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned p;
	unsigned tmp;
	unsigned *pd = page_directory;
	if(kernel_task && !(opt & MAP_PDLOCKED))
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[vdir])
	{
		p = pm_alloc_page();
		zero_page_physical(p);
		pd[vdir] = p | PAGE_WRITE | PAGE_PRESENT | (attr & PAGE_USER);
		flush_pd();
	}
	page_tables[vpage] = (phys & PAGE_MASK) | attr;
	if(!(opt & MAP_NOCLEAR))
		memset((void *)(virt&PAGE_MASK), 0, 0x1000);
#warning "should this unlock the mutex first?"
#if CONFIG_SMP
#warning "doesn't take threading into account"
	if(kernel_task && IS_KERN_MEM(virt) && !(opt & MAP_NOIPI))
		send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
#endif
	if(kernel_task && !(opt & MAP_PDLOCKED))
		mutex_release(&pd_cur_data->lock);
	return 0;
}
