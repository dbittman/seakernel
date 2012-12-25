/* Functions for unmapping addresses */
#include <kernel.h>
#include <memory.h>
#include <isr.h>
#include <task.h>
#include <swap.h>
int vm_unmap_only(unsigned virt)
{
	if(current_task && num_swapdev && current_task->num_swapped)
		swap_in_page((task_t *)current_task, virt & PAGE_MASK);
	page_tables[(virt&PAGE_MASK)/0x1000] = 0;
	__asm__ volatile ("invlpg (%0)" : : "a" (virt));
	return 0;
}

int vm_unmap(unsigned virt)
{
	/* This gives the virtual address of the table needed, and sets
	 * the correct place as zero */
	if(current_task && num_swapdev && current_task->num_swapped)
		swap_in_page((task_t *)current_task, virt & PAGE_MASK);
	unsigned p = page_tables[(virt&PAGE_MASK)/0x1000] & PAGE_MASK;
	page_tables[(virt&PAGE_MASK)/0x1000] = 0;
	__asm__ volatile ("invlpg (%0)" : : "a" (virt));
	if(p)
		pm_free_page(p);
	return 0;
}
