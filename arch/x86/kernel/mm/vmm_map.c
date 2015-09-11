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
#include <stdatomic.h>

static void map_kernel_mem(addr_t virt, addr_t phys, unsigned attr, int opt)
{
	int vpage = (virt & PAGE_MASK) / 0x1000;
	int vdir = PAGE_DIR_IDX(vpage);
	
	assert(page_directory[vdir]);
	
	addr_t zero = 0;
	if(atomic_compare_exchange_strong(&page_tables[vpage], &zero, phys)) {
		atomic_fetch_or(&page_tables[vpage], attr);
		asm volatile("invlpg (%0)"::"r" (virt) : "memory");
		if(!(opt & MAP_NOCLEAR))
			memset((void *)(virt&PAGE_MASK), 0, 0x1000);
	}
}

int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	if(!IS_KERN_MEM(virt)) {
		unsigned vpage = (virt&PAGE_MASK)/0x1000;
		unsigned vdir = PAGE_DIR_IDX(vpage);
		addr_t p;
		unsigned *pd = page_directory;
		if(pd_cur_data && !(opt & MAP_PDLOCKED) && current_process->thread_count > 1)
			mutex_acquire(&pd_cur_data->lock);
		if(!pd[vdir])
		{
			p = mm_alloc_physical_page();
			mm_zero_page_physical(p);
			pd[vdir] = p | PAGE_WRITE | PAGE_PRESENT | (attr & PAGE_USER);
			asm volatile ("invlpg (%0)"::"r" (page_tables) : "memory");
		}
		assert(!page_tables[vpage]);
		page_tables[vpage] = (phys & PAGE_MASK) | attr;
		if(pd_cur_data && !(opt & MAP_PDLOCKED) && current_process->thread_count > 1)
			mutex_release(&pd_cur_data->lock);
		asm volatile("invlpg (%0)"::"r" (virt) : "memory");
		if(!(opt & MAP_NOCLEAR))
			memset((void *)(virt&PAGE_MASK), 0, 0x1000);
	} else {
		map_kernel_mem(virt, phys, attr, opt);
	}
#if CONFIG_SMP
	if(pd_cur_data) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	return 0;
}

