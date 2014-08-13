/* generic page-fault handling */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/mm/map.h>
#include <sea/string.h>
#include <sea/cpu/processor.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>

static int do_map_page(addr_t addr, unsigned attr)
{
	/* only map a new page if one isn't already mapped. In addition,
	 * we tell the functions that the directory is locked (since it always
	 * will be) */
	addr &= PAGE_MASK;
	if(!mm_vm_get_map(addr, 0, 1))
		mm_vm_map(addr, mm_alloc_physical_page(), attr, MAP_CRIT | MAP_PDLOCKED);
	return 1;
}

static int map_in_page(addr_t address)
{
	/* check if the memory is for the heap */
	if(address >= current_task->heap_start && address <= current_task->heap_end)
		return do_map_page(address, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
	/* and check if the memory is for the stack */
	if(address >= TOP_TASK_MEM_EXEC && address < (TOP_TASK_MEM_EXEC+STACK_SIZE*2))
		return do_map_page(address, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
	return 0;
}

void mm_page_fault_handler(registers_t *regs, addr_t address, int pf_cause)
{
	/* here the story of the horrible Page Fault. If this function gets
	 * called, some part of code has accessed some dark corners of memory
	 * that don't exist. Or it has accessed something that the kernel's
	 * lazy ass decided to put off mapping until later.
	 *
	 * should this function return, the instruction will be re-tried. In
	 * theory, this doesn't matter - if this function is implemented correctly,
	 * then it will always either prevent another page fault or kill the task.
	 *
	 * if there is a pagefault in the kernel, we (almost always) assume that
	 * we're screwed. In almost all cases, a pagefault in the kernel is a
	 * bug in the kernel.
	 */
	assert(regs);
	if(pf_cause & PF_CAUSE_USER) {
		/* page fault was caused while in ring 3. We can pretend to
		 * be a second-stage interrupt handler... */
		assert(!current_task->sysregs);
		current_task->sysregs = regs;
#if CONFIG_SWAP
		/* Has the page been swapped out? NOTE: We must always check this first */
		if(current_task && num_swapdev && current_task->num_swapped && 
			swap_in_page((task_t *)current_task, address & PAGE_MASK) == 0) {
			printk(1, "[swap]: Swapped back in page %x for task %d\n", 
				   address & PAGE_MASK, current_task->pid);
			current_task->sysregs = 0;
			return;
		}
#endif
		/* check if we need to map a page for mmap, etc */
		if(mm_page_fault_test_mappings(address, pf_cause) == 0) {
			current_task->sysregs = 0;
			return;
		}
		/* if that didn't work, lets see if we should map a page
		 * for the heap, or whatever */
		mutex_acquire(&pd_cur_data->lock);
		if(map_in_page(address)) {
			mutex_release(&pd_cur_data->lock);
			current_task->sysregs = 0;
			return;
		}
		/* ...and if that didn't work, the task did something evil */
		mutex_release(&pd_cur_data->lock);

		kprintf("[mm]: %d: segmentation fault at eip=%x\n", current_task->pid, regs->eip);
		printk(0, "[mm]: %d: cause = %x, address = %x\n", current_task->pid, pf_cause, address);
		printk(0, "[mm]: %d: heap %x -> %x, flags = %x\n",
				current_task->pid, current_task->heap_start, 
				current_task->heap_end, current_task->flags);
		
		tm_kill_process(current_task->pid);
		/* this function does not return */
	} else {
		/* WARNING: TODO: this might not be safe */
		if(mm_page_fault_test_mappings(address, pf_cause) == 0)
			return;
	}
	printk(0, "[mm]: KPF: current_task=%x:%d, system=%d, flags=%x, F=%x\n",
			current_task, current_task->pid, current_task->system,
			current_task->flags, current_task->flag);
	if(!current_task) {
		if(kernel_task) {
			/* maybe a page fault while panicing? */
			cpu_interrupt_set(0);
			cpu_halt();
		}
		panic(PANIC_MEM | PANIC_NOSYNC, "early page fault (addr=%x, cause=%x)", address, pf_cause);
	}
	panic(PANIC_MEM | PANIC_NOSYNC, "page fault (addr=%x, cause=%x)", address, pf_cause);
}

