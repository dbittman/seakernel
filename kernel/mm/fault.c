/* generic page-fault handling */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/mm/map.h>
#include <sea/string.h>
#include <sea/cpu/processor.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>
#include <sea/syscall.h>
#include <sea/mm/pmm.h>
#include <sea/lib/timer.h>
#include <sea/fs/kerfs.h>
static struct timer timer;
static bool timer_init = false;
int kerfs_pfault_report(int direction, void *param, size_t size, size_t offset, size_t length, unsigned char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"    MIN      MAX    MEAN   RMEAN COUNT\n"
			"%7d %8d %7d %7d %5d\n",
			timer.min, timer.max, (uint64_t)timer.mean,
			(uint64_t)timer.recent_mean, timer.runs);
	return current;
}

static void print_mappings(void)
{
	struct process *proc = current_process;
	assert(proc->magic == PROCESS_MAGIC);
	printk(0, "           START              END   LENGTH FLAGS    INODE   OFFSET\n");
	mutex_acquire(&proc->map_lock);
	
	struct linkedentry *node;
	struct memmap *map;
	for(node = linkedlist_iter_start(&proc->mappings);
			node != linkedlist_iter_end(&proc->mappings);
			node = linkedlist_iter_next(node)) {
		map = linkedentry_obj(node);
		char flags[6];
		memset(flags, ' ', sizeof(flags));
		if(map->flags & MAP_SHARED)
			flags[0] = 'S';
		if(map->flags & MAP_FIXED)
			flags[1] = 'F';
		if(map->flags & MAP_ANONYMOUS)
			flags[2] = 'A';
		if(map->prot & PROT_WRITE)
			flags[3] = 'W';
		if(map->prot & PROT_EXEC)
			flags[4] = 'E';
		flags[5] = 0;
		printk(0, "%16x %16x %8x %s %8d %8x\n",
				map->virtual, map->virtual + map->length, map->length,
				flags, map->node->id, map->offset);
	}

	mutex_release(&proc->map_lock);
}

void mm_page_fault_handler(struct registers *regs, addr_t address, int pf_cause)
{
	if(!timer_init) {
		timer_init = true;
		timer_create(&timer, 0);
	}
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
	timer_start(&timer);
	if(pf_cause & PF_CAUSE_USER) {
		/* check if we need to map a page for mmap, etc */
		if(mm_page_fault_test_mappings(address, pf_cause) == 0) {
			timer_stop(&timer);
			return;
		}
		kprintf("[mm]: %d: segmentation fault at eip=%x\n", current_thread->tid, regs->eip);
		printk(0, "[mm]: %d: cause = %x, address = %x\n", current_thread->tid, pf_cause, address);
		print_mappings();

		tm_signal_send_thread(current_thread, SIGSEGV);
		timer_stop(&timer);
		return;
	} else if(current_process && !IS_KERN_MEM(address)) {
		/* okay, maybe we have a section of memory that a process
		 * allocated, that wasn't mapped in during sbrk. If this
		 * memory is used as a buffer, and passed to the kernel
		 * during read, for example, the kernel might try to write
		 * something to it, causing a fault. So, even though we're
		 * in the kernel, check that case. */
		if(mm_page_fault_test_mappings(address, pf_cause) == 0) {
			timer_stop(&timer);
			return;
		}
		tm_signal_send_thread(current_thread, SIGSEGV);
		timer_stop(&timer);
		return;
	}
	if(!current_thread) {
		panic(PANIC_MEM | PANIC_NOSYNC, "early page fault (addr=%x, cause=%x, from=%x)", address, pf_cause, regs->eip);
	}
	panic(PANIC_MEM | PANIC_NOSYNC, "page fault (addr=%x, cause=%x, from=%x)", address, pf_cause, regs->eip);
}

