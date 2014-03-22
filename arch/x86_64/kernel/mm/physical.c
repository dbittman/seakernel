/* Defines functions for physical memory */
#include <kernel.h>
#include <memory.h>
#include <multiboot.h>
#include <task.h>
#include <swap.h>

volatile addr_t pm_location=0;
volatile addr_t pm_stack = PM_STACK_ADDR;
volatile addr_t pm_stack_max = PM_STACK_ADDR;

volatile unsigned long pm_num_pages=0, pm_used_pages=0;
volatile addr_t highest_page=0;
volatile addr_t lowest_page=~0;

int memory_has_been_mapped=0;
volatile addr_t placement;
mutex_t pm_mutex;

void arch_mm_pm_init(addr_t start, struct multiboot *mboot)
{
	pm_location = (start + PAGE_SIZE) & PAGE_MASK;
}

addr_t arch_mm_alloc_physical_page()
{
	if(!pm_location)
		panic(PANIC_MEM | PANIC_NOSYNC, "Physical memory allocation before initilization");
	addr_t ret;
	unsigned flag=0;
	try_again:
	ret=0;
	if(current_task) {
		current_task->allocated++;
		current_task->phys_mem_usage++;
	}
	if(memory_has_been_mapped)
	{
		mutex_acquire(&pm_mutex);
		/* out of physical memory!! */
		if(pm_stack <= (PM_STACK_ADDR+sizeof(addr_t)*2)) {
			oom:
			if(current_task == kernel_task || !current_task)
				panic(PANIC_MEM | PANIC_NOSYNC, "Ran out of physical memory");
			mutex_release(&pm_mutex);
			if(OOM_HANDLER == OOM_SLEEP) {
				if(!flag++) 
					printk(0, "Warning - Ran out of physical memory in task %d\n", 
						   current_task->pid);
					tm_engage_idle();
				tm_schedule();
				goto try_again;
			} else if(OOM_HANDLER == OOM_KILL)
			{
				printk(0, "Warning - Ran out of physical memory in task %d. Killing...\n", 
					   current_task->pid);
				tm_exit(-10);
			}
			else
				panic(PANIC_MEM | PANIC_NOSYNC, "Ran out of physical memory");
		}
		pm_stack -= sizeof(addr_t);
		ret = *(addr_t *)pm_stack;
		*(addr_t *)pm_stack = 0;
		if(ret <= pm_location)
			goto oom;
		++pm_used_pages;
		mutex_release(&pm_mutex);
	} else {
		/* this isn't locked, because it is used before multitasking happens */
		ret = pm_location;
		pm_location+=PAGE_SIZE;
	}
	if(current_task)
		current_task->num_pages++;
	if(!ret)
		panic(PANIC_MEM | PANIC_NOSYNC, "found zero address in page stack (%x %x)\n", pm_stack, pm_stack_max);
	if(((ret > (unsigned)highest_page) || ret < (unsigned)lowest_page) 
		&& memory_has_been_mapped)
		panic(PANIC_MEM | PANIC_NOSYNC, "found invalid address in page stack: %x\n", ret);
	return ret;
}

addr_t arch_mm_alloc_physical_page_zero()
{
	addr_t ret = mm_alloc_physical_page();
	if(kernel_state_flags & KSF_PAGING)
		memset((void *)(ret + PHYS_PAGE_MAP), 0, 0x1000);
	else
		memset((void *)ret, 0, 0x1000);
	return ret;
}

void arch_mm_free_physical_page(addr_t addr)
{
	if(!(kernel_state_flags & KSF_PAGING))
		panic(PANIC_MEM | PANIC_NOSYNC, "Called free page without paging environment");
	if(addr < pm_location || (((addr > highest_page) || addr < lowest_page)
		&& memory_has_been_mapped)) {
		panic(PANIC_MEM | PANIC_NOSYNC, "tried to free invalid physical address (%x)", addr);
		return;
	}
	assert(addr);
	if(current_task) {
		current_task->freed++;
		current_task->phys_mem_usage--;
	}
	mutex_acquire(&pm_mutex);
	if(pm_stack_max <= pm_stack)
	{
		if(!memory_has_been_mapped)
			mm_vm_map(pm_stack_max, mm_alloc_physical_page(), PAGE_PRESENT | PAGE_WRITE, 0);
		else
			mm_vm_map(pm_stack_max, addr, PAGE_PRESENT | PAGE_WRITE, 0);
		memset((void *)pm_stack_max, 0, PAGE_SIZE);
		pm_stack_max += PAGE_SIZE;
		if(!memory_has_been_mapped) goto add;
	} else {
		add:
		assert(*(addr_t *)(pm_stack) = addr);
		pm_stack += sizeof(addr_t);
		--pm_used_pages;
		assert(*(addr_t *)(pm_stack - sizeof(addr_t)) == addr);
	}
	mutex_release(&pm_mutex);
	if(current_task && current_task->num_pages)
		current_task->num_pages--;
}
