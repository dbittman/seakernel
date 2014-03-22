/* Defines functions for virtual memory management */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>
#include <sea/cpu/interrupt.h>
#if CONFIG_SMP
#include <imps-x86.h>
#endif
#include <sea/mm/vmm.h>
volatile page_dir_t *kernel_dir=0, *minimal_directory=0;
unsigned int cr0temp;
int id_tables=0;
struct pd_data *pd_cur_data = (struct pd_data *)PDIR_DATA;

addr_t id_mapped_location;

static addr_t vm_init_directory(addr_t id_map_to)
{
	/* Create kernel directory. 
	 * This includes looping upon itself for self-reference */
	page_dir_t *pd;
	pd = (page_dir_t *)mm_alloc_physical_page();
	memset(pd, 0, 0x1000);
	pd[1022] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	unsigned int *pt = (unsigned int *)(pd[1022] & PAGE_MASK);
	memset(pt, 0, 0x1000);
	pt[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	pd[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	/* we don't create an accounting page for this one, since this page directory is only
	 * temporary */
	/* Identity map the kernel */
	unsigned mapper=0, i;
	while(mapper <= PAGE_DIR_IDX(((id_map_to&PAGE_MASK)+0x1000)/0x1000)) {
		pd[mapper] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_USER;
		pt = (unsigned int *)(pd[mapper] & PAGE_MASK);
		memset(pt, 0, 0x1000);
		/* we map as user for now, since the init() function runs in
		 * ring3 for a short amount of time and needs read access to the
		 * kernel code. This is later re-mapped by the kernel idle 
		 * process with proper protection flags */
		for(i=0;i<1024;i++)
			pt[i] = (mapper*1024*0x1000 + 0x1000*i) | PAGE_PRESENT 
						| PAGE_USER;
		mapper++;
	}
	id_tables=mapper;
#if CONFIG_SMP
	id_map_apic(pd);
#endif
	/* map in the signal return inject code. we need to do this, because
	 * user code may not run the the kernel area of the page directory */
	unsigned sig_pdi = PAGE_DIR_IDX(SIGNAL_INJECT / PAGE_SIZE);
	unsigned sig_tbi = PAGE_TABLE_IDX(SIGNAL_INJECT / PAGE_SIZE);
	assert(!pd[sig_pdi]);
	pd[sig_pdi] = ((unsigned)(pt=(unsigned *)mm_alloc_physical_page()) | PAGE_PRESENT | PAGE_USER);
	memset(pt, 0, 0x1000);
	pt[sig_tbi] = (unsigned)mm_alloc_physical_page() | PAGE_PRESENT | PAGE_USER;
	memcpy((void *)(pt[sig_tbi] & PAGE_MASK), (void *)signal_return_injector, SIGNAL_INJECT_SIZE);
	/* premap the tables of stuff so that cloning directories works properly */
	/* Pre-map the heap's tables */
	unsigned heap_pd_idx = PAGE_DIR_IDX(KMALLOC_ADDR_START / 0x1000);
	for(i=heap_pd_idx;i<(int)PAGE_DIR_IDX(KMALLOC_ADDR_END / 0x1000);i++)
	{
		pd[i] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* Pre-map the PMM's tables */
	unsigned pm_pd_idx = PAGE_DIR_IDX(PM_STACK_ADDR / 0x1000);
	for(i=pm_pd_idx;i<(int)PAGE_DIR_IDX(PM_STACK_ADDR_TOP / 0x1000);i++)
	{
		pd[i] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* Pre-map the mmdev's tables */
	pm_pd_idx = PAGE_DIR_IDX(DEVICE_MAP_START / 0x1000);
	for(i=pm_pd_idx;i<(int)PAGE_DIR_IDX(DEVICE_MAP_END / 0x1000);i++)
	{
		pd[i] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	return (addr_t)pd;
}

/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
void arch_mm_vm_init(addr_t id_map_to)
{
	id_mapped_location=id_map_to;
	/* Register some stuff... */
	arch_interrupt_register_handler (14, (isr_t)&arch_mm_page_fault, 0);
	
	page_dir_t *pd = (page_dir_t *)vm_init_directory(id_map_to);
	minimal_directory = pd;
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (pd));
	/* Enable */
	enable_paging();
	
	set_ksf(KSF_PAGING);
}

/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void arch_mm_vm_init_2()
{
	setup_kernelstack(id_tables);
	printk(0, "[mm]: cloning directory for boot processor\n");
	primary_cpu->kd = mm_vm_clone(page_directory, 0);
	primary_cpu->kd_phys = primary_cpu->kd[1023] & PAGE_MASK;
	printk(0, "[mm]: cloned\n");
	kernel_dir = primary_cpu->kd;
	/* can't call vm_switch, because we'll end up with the stack like it was
	 * when we call vm_clone! So, we have to assume an invalid stack until
	 * this function returns */
	asm ("mov %0, %%cr3; nop; nop" :: "r" ((addr_t)kernel_dir[1023] & PAGE_MASK));
	flush_pd();
	printk(0, "[mm]: switched\n");
}

void arch_mm_vm_switch_context(page_dir_t *n/*VIRTUAL ADDRESS*/)
{
	/* n[1023] is the mapped bit that loops to itself */
 	asm ("mov %0, %%cr3" :: "r" ((addr_t)n[1023] & PAGE_MASK));
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	if(kernel_task && !locked)
		mutex_acquire(&pd_cur_data->lock);
	unsigned ret = page_tables[vp] & PAGE_MASK;
	if(kernel_task && !locked)
		mutex_release(&pd_cur_data->lock);
	if(p)
		*p = ret;
	return ret;
}

void arch_mm_vm_set_attrib(addr_t v, short attr)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return;
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	(page_tables[vp] &= PAGE_MASK);
	(page_tables[vp] |= attr);
	asm("invlpg (%0)"::"r" (v));
#if CONFIG_SMP
	if(kernel_task) {
		if(IS_KERN_MEM(v))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(v) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
}

unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	if(kernel_task && !locked)
		mutex_acquire(&pd_cur_data->lock);
	unsigned ret = page_tables[vp] & ATTRIB_MASK;
	if(kernel_task && !locked)
		mutex_release(&pd_cur_data->lock);
	if(p)
		*p = ret;
	return ret;
}
