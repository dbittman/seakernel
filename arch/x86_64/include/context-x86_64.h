#ifndef __CONTEXT_X86_64_H
#define __CONTEXT_X86_64_H
/* Functions for scheduling tasks */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>

static void _overflow(char *type)
{
	printk(5, "%s overflow occurred in task %d (esp=%x, ebp=%x, heap_end=%x). Killing...\n", 
		   type, current_task->pid, current_task->esp, current_task->ebp, 
		current_task->heap_end);
	#if DEBUG
	panic(0, "Overflow");
	#endif
	task_suicide();
}

__attribute__((always_inline)) inline static void store_context()
{
	asm("mov %%rsp, %0" : "=r"(current_task->esp));
	asm("mov %%rbp, %0" : "=r"(current_task->ebp));
	/* Check for stack and heap overflow */
	if(!current_task->esp || (!(current_task->esp >= TOP_TASK_MEM_EXEC && current_task->esp < TOP_TASK_MEM) 
		&& !(current_task->esp >= KMALLOC_ADDR_START && current_task->esp < KMALLOC_ADDR_END)))
		_overflow("stack");
	if(current_task->heap_end && current_task->heap_end >= TOP_USER_HEAP)
		_overflow("heap");
	/* the exiting task has fully 'exited' and has now scheduled out of
	 * itself. It will never be scheduled again, and the page directory
	 * will never be accessed again */
	if(current_task->flags & TF_DYING)
		current_task->flags |= TF_BURIED;
	current_task->syscall_count = 0;
}

__attribute__((always_inline)) inline static void restore_context(task_t *n)
{
	/* Update some last-minute things. The stack. */
	set_kernel_stack(current_tss, (n->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE)) & ~0xF);
	/* keep track of when we got to run */
}

__attribute__((always_inline)) inline static void context_switch(task_t *n)
{
	asm("         \
	mov %1, %%rsp;       \
	mov %2, %%rbp;       \
	mov %3, %%cr3;"
	: : "r"(0), "r"(n->esp), "r"(n->ebp), 
		"r"(n->pd[PML4_IDX(PHYSICAL_PML4_INDEX/0x1000)]&PAGE_MASK) : "rax");
}

__attribute__((always_inline)) 
inline static int engage_new_stack(task_t *task, task_t *parent)
{
	assert(parent == current_task);
	u64int ebp;
	u64int esp;
	asm("mov %%rsp, %0" : "=r"(esp));
	asm("mov %%rbp, %0" : "=r"(ebp));
	if(esp > TOP_TASK_MEM) {
		task->esp=(esp-parent->kernel_stack) + task->kernel_stack;
		task->ebp=(ebp-parent->kernel_stack) + task->kernel_stack;
		task->sysregs = (parent->sysregs - parent->kernel_stack) + task->kernel_stack;
		copy_update_stack(task->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		task->sysregs = parent->sysregs;
		task->esp=esp;
		task->ebp=ebp;
		return 0;
	}
}

#endif
