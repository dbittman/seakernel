/* Defines functions for virtual memory management */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
char paging_enabled=0;
volatile page_dir_t *kernel_dir=0;
unsigned int cr0temp;
int id_tables=0;
struct pd_data *pd_cur_data = (struct pd_data *)PDIR_DATA;
extern void id_map_apic(page_dir_t *);
/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
void vm_init(unsigned id_map_to)
{
	/* Register some stuff... */
	register_interrupt_handler (14, (isr_t)&page_fault);

	/* Create kernel directory. 
	 * This includes looping upon itself for self-reference */
	page_dir_t *pd;
	pd = (page_dir_t *)pm_alloc_page();
	memset(pd, 0, 0x1000);
	pd[1022] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	unsigned int *pt = (unsigned int *)(pd[1022] & PAGE_MASK);
	memset(pt, 0, 0x1000);
	pt[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	pd[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	/* we don't create an accounting page for this one, since this page directory is only
	 * temporary */
	/* Identity map the kernel */
	unsigned mapper=0;
	int i;
	while(mapper <= PAGE_DIR_IDX(((id_map_to&PAGE_MASK)+0x1000)/0x1000)) {
		pd[mapper] = pm_alloc_page() | PAGE_PRESENT | PAGE_USER;
		pt = (unsigned int *)(pd[mapper] & PAGE_MASK);
		memset(pt, 0, 0x1000);
		/* we map as user for now, since the init() function runs in
		 * ring0 for a short amount of time and needs read access to the
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
	/* Pre-map the heap's tables */
	unsigned heap_pd_idx = PAGE_DIR_IDX(KMALLOC_ADDR_START / 0x1000);
	for(i=heap_pd_idx;i<(int)PAGE_DIR_IDX(KMALLOC_ADDR_END / 0x1000);i++)
	{
		pd[i] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* Now map in the physical page stack so we have memory to use */
	for(i=PAGE_DIR_IDX((PM_STACK_ADDR/0x1000));
		i<(int)PAGE_DIR_IDX(PM_STACK_ADDR_TOP/0x1000);i++)
	{
		pd[i] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (pd));
	/* Enable */
	enable_paging();
	paging_enabled=1;
	memset(0, 0, 0x1000);
}

/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void vm_init_2()
{
	setup_kernelstack(id_tables);
#if CONFIG_SMP
	int i=0;
	while(i < cpu_array_num)
	{
		printk(0, "[mm]: cloning directory for processor %d\n", cpu_array[i].apicid);
		cpu_array[i].kd = vm_clone(page_directory, 0);
		cpu_array[i].kd_phys = cpu_array[i].kd[1023] & PAGE_MASK;
		i++;
	}
#else
	primary_cpu->kd = vm_clone(page_directory, 0);
	primary_cpu->kd_phys = primary_cpu->kd[1023] & PAGE_MASK;
#endif
	kernel_dir = primary_cpu->kd;
	vm_switch(primary_cpu->kd);
	printk(0, "[mm]: using cloned directory\n");
}

void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/)
{
	/* n[1023] is the mapped bit that loops to itself */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (n[1023]&PAGE_MASK));
}

unsigned int vm_do_getmap(unsigned v, unsigned *p, unsigned locked)
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

unsigned int vm_setattrib(unsigned v, short attr)
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

unsigned int vm_do_getattrib(unsigned v, unsigned *p, unsigned locked)
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
