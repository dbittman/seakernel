/* Defines functions for virtual memory management */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>
volatile page_dir_t *kernel_dir=0;
int id_tables=0;
struct pd_data *pd_cur_data = (struct pd_data *)PDIR_DATA;
extern void id_map_apic(page_dir_t *);
/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
void vm_init(addr_t id_map_to)
{
	/* Register some stuff... */
	register_interrupt_handler (14, (isr_t)&page_fault, 0);

	/* Create kernel directory. 
	 * This includes looping upon itself for self-reference */
	
	/* we don't create an accounting page for this one, since this page directory is only
	 * temporary */
	
	/* Identity map the kernel */
	
#if CONFIG_SMP
	id_map_apic(pd);
#endif
	/* map in the signal return inject code. we need to do this, because
	 * user code may not run the the kernel area of the page directory */
	
	/* Pre-map the heap's tables */
	
	/* Now map in the physical page stack so we have memory to use */
	
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	
	/* Enable paging */
	
	set_ksf(KSF_PAGING);
	memset(0, 0, 0x1000);
}

/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void vm_init_2()
{
	setup_kernelstack(id_tables);
#if CONFIG_SMP
	unsigned int i=0;
	while(i < cpu_array_num)
	{
		printk(0, "[mm]: cloning directory for processor %d (%x)\n", cpu_array[i].apicid, &cpu_array[i]);
		page_dir_t *pd = vm_clone(page_directory, 0);
		cpu_array[i].kd_phys = pd[1023] & PAGE_MASK;
		cpu_array[i].kd = pd;
		i++;
	}
#else
	primary_cpu->kd = vm_clone(page_directory, 0);
	primary_cpu->kd_phys = primary_cpu->kd[1023] & PAGE_MASK;
#endif
	kernel_dir = primary_cpu->kd;
	vm_switch((page_dir_t *)primary_cpu->kd);
}

void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/)
{
	/* n[1023] is the mapped bit that loops to itself */
	//__asm__ volatile ("mov %0, %%cr3" : : "r" (n[1023]&PAGE_MASK));
}

addr_t vm_do_getmap(addr_t v, addr_t *p, unsigned locked)
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

unsigned int vm_setattrib(addr_t v, short attr)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	(page_tables[vp] &= PAGE_MASK);
	(page_tables[vp] |= attr);
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return 0;
}

unsigned int vm_do_getattrib(addr_t v, unsigned *p, unsigned locked)
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
