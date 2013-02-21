/* Functions for mapping of addresses */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>

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
	if(kernel_task && !(opt & MAP_PDLOCKED))
		mutex_release(&pd_cur_data->lock);
	return 0;
}
