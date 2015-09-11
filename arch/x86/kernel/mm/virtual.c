/* Defines functions for virtual memory management */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>

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
extern void *boot_directory;
extern void *boot_pagetable;
static char alignas(0x1000) tmp[0x1000];
static addr_t vm_init_directory(addr_t id_map_to)
{
	addr_t **pd = (void *)&boot_directory;
	addr_t *pt = (void *)&boot_pagetable;
	memset(pt, 0, 0x1000);
	pd[1022] = (void *)(((addr_t)&boot_pagetable - 0xC0000000) | PAGE_PRESENT | PAGE_WRITE);
	pt[1023] = ((addr_t)&boot_directory - 0xC0000000) | PAGE_PRESENT | PAGE_WRITE;
	pd[1023] = (void *)pt[1023];

	flush_pd();

	int pdindex = (0x70000000 >> 22);
	int ptindex = (0x70000000 >> 12) & 0x3FF;
	pd = (void *)0xFFFFF000;
	if(!pd[pdindex]) {
		printk(0, "mapping new table %x\n", tmp);
		pt[pdindex] = ((addr_t)tmp - 0xC0000000) | PAGE_PRESENT | PAGE_WRITE;
		flush_pd();
		
	}
	for(;;);
	pt = (void *)(0xFFC00000 + (0x400 * pdindex));

	pt[ptindex] = 0xA0000000 | PAGE_PRESENT | PAGE_WRITE;


	for(;;);
	return 0;
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
	minimal_context.root_physical = (addr_t)pd;
	minimal_context.root_virtual = (addr_t)page_directory;
	minimal_context.magic = CONTEXT_MAGIC;
	mutex_create(&minimal_context.lock, MT_NOSCHED);
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (pd));
	__asm__ volatile ("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0":::"eax");
	
	set_ksf(KSF_PAGING);
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
void arch_mm_vm_switch_context(struct vmm_context *context)
{
	assert(context->magic == CONTEXT_MAGIC);
 	__asm__ __volatile__ ("mov %0, %%cr3" :: "r" (context->root_physical):"memory");
}
/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void arch_mm_vm_init_2(void)
{
	//mm_vm_clone(&minimal_context, &kernel_context);
	arch_mm_vm_switch_context(&kernel_context);
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(pd_cur_data && !locked && current_process->thread_count > 1)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data && !locked && current_process->thread_count > 1)
			mutex_release(&pd_cur_data->lock);
		return 0;
	}
	unsigned ret = page_tables[vp] & PAGE_MASK;
	if(pd_cur_data && !locked && current_process->thread_count > 1)
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
	if(pd_cur_data && current_process->thread_count > 1)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data && current_process->thread_count > 1)
			mutex_release(&pd_cur_data->lock);
		return;
	}
	unsigned ret = page_tables[vp] & PAGE_MASK;
	(page_tables[vp] &= PAGE_MASK);
	(page_tables[vp] |= attr);
	__asm__ __volatile__ ("invlpg (%0)"::"r" (v));
#if CONFIG_SMP
	if(pd_cur_data) {
		if(IS_KERN_MEM(v))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(v)))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(pd_cur_data && current_process->thread_count > 1)
		mutex_release(&pd_cur_data->lock);
}

unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(pd_cur_data && !locked && current_process->thread_count > 1)
		mutex_acquire(&pd_cur_data->lock);
	if(!pd[pt_idx])
	{
		if(pd_cur_data && !locked && current_process->thread_count > 1)
			mutex_release(&pd_cur_data->lock);
		return 0;
	}
	unsigned ret = page_tables[vp] & ATTRIB_MASK;
	if(pd_cur_data && !locked && current_process->thread_count > 1)
		mutex_release(&pd_cur_data->lock);
	if(p)
		*p = ret;
	return ret;
}

