/* Defines functions for virtual memory management */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
char paging_enabled=0;
volatile page_dir_t *kernel_dir=0;
unsigned *page_directory=(unsigned *)DIR_PHYS, *page_tables=(unsigned *)TBL_PHYS;
unsigned int cr0temp;
int id_tables=0;
extern void id_map_apic(page_dir_t *);
/* This function will setup a paging environment with a basic page dir, enough to process the memory map passed by grub */
void vm_init(unsigned id_map_to)
{
	/* Register some stuff... */
	register_interrupt_handler (14, (isr_t)&page_fault);

	/* Create kernel directory. This includes looping upon itself for self-reference */
	page_dir_t *pd;
	pd = (page_dir_t *)pm_alloc_page();
	memset(pd, 0, 0x1000);
	pd[1022] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
	unsigned int *pt = (unsigned int *)(pd[1022] & PAGE_MASK);
	memset(pt, 0, 0x1000);
	pt[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	pd[1023] = (unsigned int) pd | PAGE_PRESENT | PAGE_WRITE;
	
	/* Identity map the kernel */
	unsigned mapper=0;
	int i;
	while(mapper <= PAGE_DIR_IDX(((id_map_to&PAGE_MASK)+0x1000)/0x1000)) {
		pd[mapper] = pm_alloc_page() | PAGE_PRESENT | PAGE_USER;
		pt = (unsigned int *)(pd[mapper] & PAGE_MASK);
		memset(pt, 0, 0x1000);
		for(i=0;i<1024;i++)
			pt[i] = (mapper*1024*0x1000 + 0x1000*i) | PAGE_PRESENT | ((mapper+i) ? PAGE_USER : PAGE_USER);
		mapper++;
	}
	id_tables=mapper;
#ifdef CONFIG_SMP
	id_map_apic(pd);
#endif
	/* Pre-map the heap's tables */
	unsigned heap_pd_idx = PAGE_DIR_IDX(KMALLOC_ADDR_START / 0x1000);
	for(i=heap_pd_idx;i<(int)PAGE_DIR_IDX(KMALLOC_ADDR_END / 0x1000);i++)
	{
		pd[i] = pm_alloc_page() | PAGE_PRESENT | PAGE_USER;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* Now map in the physical page stack so we have memory to use */
	for(i=PAGE_DIR_IDX((PM_STACK_ADDR/0x1000));i<(int)PAGE_DIR_IDX(PM_STACK_ADDR_TOP/0x1000);i++)
	{
		pd[i] = pm_alloc_page() | PAGE_PRESENT | PAGE_WRITE;
		pt = (unsigned int *)(pd[i] & PAGE_MASK);
		memset(pt, 0, 0x1000);
	}
	/* CR3 requires the physical address, so we directly set it because we have the physical address */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (pd));
	/* Enable */
	enable_paging();
	paging_enabled=1;
	memset(0, 0, 0x1000);/* HACK */
}

/* This relocates the stack to a safe place which is copied upon clone, and creates a new directory that is...well, complete */
void vm_init_2()
{
	setup_kernelstack(id_tables);
	page_dir_t *c = vm_clone(page_directory, 0);
	kernel_dir = c;
	vm_switch(c);
}

void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/)
{
	/* n[1023] is the mapped bit that loops to itself */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (n[1023]&PAGE_MASK));
}

unsigned int vm_getmap(unsigned v, unsigned *p)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	if(p)
		*p = page_tables[vp] & PAGE_MASK;
	return (page_tables[vp] & PAGE_MASK);
}

unsigned int vm_setattrib(unsigned v, short attr)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	(page_tables[vp] &= PAGE_MASK);
	(page_tables[vp] |= attr);
	return 0;
}

unsigned int vm_getattrib(unsigned v, unsigned *p)
{
	unsigned *pd = page_directory;
	unsigned int vp = (v&PAGE_MASK) / 0x1000;
	unsigned int pt_idx = PAGE_DIR_IDX(vp);
	if(!pd[pt_idx])
		return 0;
	if(p)
		*p = page_tables[vp] & ATTRIB_MASK;
	return (page_tables[vp] & ATTRIB_MASK);
}
