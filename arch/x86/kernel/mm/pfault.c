#include <kernel.h>
#include <memory.h>
#include <mmfile.h>
#include <task.h>
#include <swap.h>
#include <elf.h>
void print_pfe(int x, registers_t *regs, unsigned cr2)
{
	assert(regs);
	printk (x, "Woah! Page Fault at 0x%x, faulting address 0x%x\n", regs->eip, cr2);
	if(!(regs->err_code & 1))
		printk (x, "Present, ");
	else
		printk(x, "Non-present, ");
	if(regs->err_code & 2)
		printk (x, "read-only, ");
	printk(x, "while in ");
	if(regs->err_code & 4)
		printk (x, "User");
	else
		printk(x, "Supervisor");
	printk(x, " mode");
	printk(x, "\nIn function");
	const char *g = elf_lookup_symbol (regs->eip, &kernel_elf);
	printk(x, " [0x%x] %s\n", regs->eip, g ? g : "(unknown)");
}
#define USER_TASK (err_code & 0x4)

int do_map_page(unsigned addr, unsigned attr)
{
	addr &= PAGE_MASK;
	if(!vm_getmap(addr, 0))
		vm_map(addr, pm_alloc_page(), attr, MAP_CRIT);
	return 1;
}

int map_in_page(unsigned int cr2, unsigned err_code)
{
	if(cr2 >= current_task->heap_start && cr2 <= current_task->heap_end) {
		do_map_page(cr2, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
		memset((void *)(cr2&PAGE_MASK), 0, PAGE_SIZE);
		return 1;
	}
	if(cr2 >= TOP_TASK_MEM_EXEC && cr2 < (TOP_TASK_MEM_EXEC+STACK_SIZE*2))
		return do_map_page(cr2, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
	if(USER_TASK)
		return 0;
	if((cr2 >= KMALLOC_ADDR_START && cr2 < KMALLOC_ADDR_END))
		return do_map_page(cr2, PAGE_PRESENT | PAGE_USER);
	return 0;
}

void print_pf(int x, registers_t *regs, unsigned cr2)
{
	printk(x, "Occured in task %d.\n\tstate=%d, flags=%d, F=%d, magic=%x.\n\tlast syscall=%d, kernel is %slocked", current_task->pid, current_task->state, current_task->flags, current_task->flag, current_task->magic, current_task->last, current_task->critical ? "" : "un");
	if(current_task->system) 
		printk(x, ", in syscall %d", current_task->system);
	printk(x, "\n");
}
#if DEBUG
#define dont_panic 0
#else
#define dont_panic 1
#endif
void page_fault(registers_t regs)
{
	uint32_t cr2, err_code = regs.err_code;
	__asm__ volatile ("mov %%cr2, %0" : "=r" (cr2));
#if CONFIG_SWAP
	/* Has the page been swapped out? NOTE: We must always check this first */
	if(current_task && num_swapdev && current_task->num_swapped && 
			swap_in_page((task_t *)current_task, cr2 & PAGE_MASK) == 0) {
		printk(1, "[swap]: Swapped back in page %x for task %d\n", 
			cr2 & PAGE_MASK, current_task->pid);
		return;
	}
#endif
	
	if(pfault_mmf_check(err_code, cr2))
		return;
	
	if(map_in_page(cr2, err_code))
		return;
	__super_cli();
	if(USER_TASK || dont_panic)
	{
		printk(!(dont_panic) ? 5 : 0, "[pf]: Invalid Memory Access in task %d: eip=%x addr=%x flags=%x\n", 
			current_task->pid, regs.eip, cr2, err_code);
		print_pf(0, &regs, cr2);
		kprintf("[pf]: Segmentation Fault\n");
		kill_task(current_task->pid);
		return;
	}
	print_pfe(5, &regs, cr2);
	printk(5, "\n");
	print_pf(5, &regs, cr2);
	if(!current_task) {
		kprintf("\n");
		if(kernel_task)
		{
			/* Page fault while panicing */
			super_cli();
			for(;;) asm("nop");
		}
		panic(PANIC_MEM | PANIC_NOSYNC, "Early Page Fault");
	}
	
	panic(PANIC_MEM | PANIC_NOSYNC, "Page Fault");
}
