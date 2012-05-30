/* Defines functions for physical memory */
#include <kernel.h>
#include <memory.h>
#include <multiboot.h>
#include <task.h>
#include <swap.h>

volatile unsigned int pm_location=0;
volatile unsigned int pm_stack = PM_STACK_ADDR;
volatile unsigned int pm_stack_max = PM_STACK_ADDR;

volatile unsigned pm_num_pages=0, pm_used_pages=0;
volatile unsigned highest_page=0;
volatile unsigned lowest_page=~0;

int memory_has_been_mapped=0;
volatile int mmu_ready=0;
volatile int placement;
mutex_t pm_mutex;
extern unsigned int end;
void pm_init(int start, struct multiboot *mboot)
{
	pm_location = (start + PAGE_SIZE) & PAGE_MASK;
}

int __pm_alloc_page(char *file, int line)
{
	if(!pm_location)
		panic(PANIC_MEM | PANIC_NOSYNC, "Physical memory allocation before initilization");
	unsigned ret;
	unsigned flag=0;
	try_again:
	ret=0;
	mutex_on(&pm_mutex);
	if(paging_enabled)
	{
		if(pm_stack <= (PM_STACK_ADDR+sizeof(unsigned)*2)) {
			if(current_task == kernel_task || !current_task) {
				set_current_task_dp(0); /* So we don't try to sync */
				panic(PANIC_MEM | PANIC_NOSYNC, "Ran out of physical memory");
			}
			mutex_off(&pm_mutex);
			if(OOM_HANDLER == OOM_SLEEP) {
				if(!flag++) 
					printk(0, "Warning - Ran out of physical memory in task %d\n", current_task->pid);
				task_full_uncritical();
				__engage_idle();
				force_schedule();
				goto try_again;
			} else if(OOM_HANDLER == OOM_KILL)
			{
				printk(0, "Warning - Ran out of physical memory in task %d. Killing...\n", current_task->pid);
				exit(-10);
			}
			else
				panic(PANIC_MEM | PANIC_NOSYNC, "Ran out of physical memory");
		}
		++pm_used_pages;
		pm_stack -= sizeof(unsigned int);
		ret = *(unsigned int *)pm_stack;
	} else {
		++pm_used_pages;
		ret = pm_location;
		pm_location+=PAGE_SIZE;
	}
	
	/* Now, I know what you're thinking - Infinite recursion!
	 * Well, the only way for that to happen is if the pm page stack is filled with
	 * zeros, which can happen for 1 of two reasons: (1) It got overwritten, and
	 * (2) some huge error. Either way we're totally fucked. So it doesn't matter */
	mutex_off(&pm_mutex);
	if(current_task)
		current_task->num_pages++;
	if(!ret) {
		printk(1, "[pmm]: BUG: found zero address in page stack\n");
		return pm_alloc_page();
	}
	if(((ret > (unsigned)highest_page) || ret < (unsigned)lowest_page) && memory_has_been_mapped) {
		printk(1, "[pmm]: BUG: found invalid address in page stack: %x\n", ret);
		return pm_alloc_page();
	}
	return ret;
}

void pm_free_page(unsigned int addr)
{
	if(!paging_enabled)
		panic(PANIC_MEM | PANIC_NOSYNC, "Called free page without paging environment");
	if(addr < pm_location) {
		return;
	}
	mutex_on(&pm_mutex);
	/* Ignore invalid page frees (like ones above a number never allocated in the
	 * first place. But only do this after the MM has been fully set up
	 * so memory map processing still works. */
	if(((addr > highest_page) || addr < lowest_page) && memory_has_been_mapped)
		panic(PANIC_MEM | PANIC_NOSYNC, "Page was freed at %x! Impossible!", addr);
	if(pm_stack_max <= pm_stack)
	{
		vm_map(pm_stack_max, addr, PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
		memset((void *)pm_stack_max, 0, PAGE_SIZE);
		pm_stack_max += PAGE_SIZE;
	} else {
		*(unsigned int *)(pm_stack) = addr;
		pm_stack += sizeof(unsigned int);
		--pm_used_pages;
	}
	if(current_task && current_task->num_pages)
		current_task->num_pages--;
	mutex_off(&pm_mutex);
}
