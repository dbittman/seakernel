/* Defines functions for virtual memory management */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/vsprintf.h>
#include <sea/syscall.h>
#if CONFIG_SMP
#include <sea/cpu/imps-x86.h>
#endif
#include <sea/mm/vmm.h>
#include <sea/asm/system.h>
int id_tables=0;

addr_t id_mapped_location;
void setup_kernelstack();
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
	/* Pre-map the contiguous tables */
	pm_pd_idx = PAGE_DIR_IDX(CONTIGUOUS_VIRT_START / 0x1000);
	for(i=pm_pd_idx;i<(int)PAGE_DIR_IDX(CONTIGUOUS_VIRT_END / 0x1000);i++)
	{
		pd[i] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	return (addr_t)pd;
}

/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
static struct vmm_context minimal_context;
void arch_mm_vm_init(addr_t id_map_to)
{
	id_mapped_location=id_map_to;
	/* Register some stuff... */
	cpu_interrupt_register_handler (14, &arch_mm_page_fault_handle);
	
	page_dir_t *pd = (page_dir_t *)vm_init_directory(id_map_to);
	minimal_context.root_physical = pd;
	minimal_context.root_virtual = page_directory;
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (pd));
	__asm__ volatile ("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0":::"eax");
	
	set_ksf(KSF_PAGING);
}

/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void arch_mm_vm_init_2()
{
	printk(0, "[mm]: cloning directory for boot processor\n");
	mm_vm_clone(&minimal_context, &kernel_context);
	printk(0, "[mm]: cloned\n");
	arch_mm_vm_switch_context(&kernel_context);
}

void arch_mm_vm_switch_context(struct vmm_context *context)
{
 	asm ("mov %0, %%cr3" :: "r" (context->root_physical):"memory");
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	/* TODO: generic "tasking system up" flag */
	if(pd_cur_data && !locked)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data && !locked)
			mutex_release(&pd_cur_data->lock);
		return 0;
	}
	unsigned ret = page_tables[vp] & PAGE_MASK;
	if(pd_cur_data && !locked)
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
	if(pd_cur_data)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data)
			mutex_release(&pd_cur_data->lock);
		return;
	}
	unsigned ret = page_tables[vp] & PAGE_MASK;
	(page_tables[vp] &= PAGE_MASK);
	(page_tables[vp] |= attr);
	asm("invlpg (%0)"::"r" (v));
#if CONFIG_SMP
	if(pd_cur_data) {
		if(IS_KERN_MEM(v))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(v) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(pd_cur_data)
		mutex_release(&pd_cur_data->lock);
}

unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(pd_cur_data && !locked)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data && !locked)
			mutex_release(&pd_cur_data->lock);
		return 0;
	}
	unsigned ret = page_tables[vp] & ATTRIB_MASK;
	if(pd_cur_data && !locked)
		mutex_release(&pd_cur_data->lock);
	if(p)
		*p = ret;
	return ret;
}

