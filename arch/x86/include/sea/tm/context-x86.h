#ifndef __CONTEXT_X86_H
#define __CONTEXT_X86_H
/* Functions for scheduling tasks */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>

#define current_tss (&((cpu_t *)current_task->cpu)->arch_cpu_data.tss)

static void _overflow(char *type)
{
	printk(5, "%s overflow occurred in task %d (esp=%x, ebp=%x, heap_end=%x). Killing...\n", 
		   type, current_task->pid, current_task->esp, current_task->ebp, 
		current_task->heap_end);
	#if DEBUG
	panic(0, "Overflow");
	#endif
	tm_process_suicide();
}

__attribute__((always_inline)) inline static void store_context_fork(task_t *task)
{
	asm("mov %%ebx, %0" : "=r"(task->preserved[0]));
	asm("mov %%edi, %0" : "=r"(task->preserved[1]));
	asm("mov %%esi, %0" : "=r"(task->preserved[2]));
}

__attribute__((always_inline)) inline static void store_context()
{
	asm("mov %%esp, %0" : "=r"(current_task->esp));
	asm("mov %%ebp, %0" : "=r"(current_task->ebp));
	
	asm("mov %%ebx, %0" : "=r"(current_task->preserved[0]));
	asm("mov %%edi, %0" : "=r"(current_task->preserved[1]));
	asm("mov %%esi, %0" : "=r"(current_task->preserved[2]));
	/* TODO: There is a lot of overhead here, because we don't need
	 * to do this for every task. For now, this works, but it needs
	 * to be fixed. */
	if(((cpu_t *)current_task->cpu)->flags & CPU_FXSAVE || ((cpu_t *)current_task->cpu)->flags & CPU_SSE || ((cpu_t *)current_task->cpu)->flags & CPU_FPU)
		__asm__ __volatile__("fxsave (%0)"
			:: "r" (ALIGN(current_task->fpu_save_data, 16)));
	/* Check for stack and heap overflow */
	if(!current_task->esp || (!(current_task->esp >= TOP_TASK_MEM_EXEC && current_task->esp < TOP_TASK_MEM) 
		&& !(current_task->esp >= KMALLOC_ADDR_START && current_task->esp < KMALLOC_ADDR_END)))
		_overflow("stack");
	if(current_task->heap_end && current_task->heap_end >= TOP_USER_HEAP)
		_overflow("heap");
}

__attribute__((always_inline)) inline static void restore_context(task_t *n)
{
	/* Update some last-minute things. The stack. */
	set_kernel_stack(current_tss, n->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));

	if(((cpu_t *)n->cpu)->flags & CPU_SSE || ((cpu_t *)n->cpu)->flags & CPU_FPU)
		__asm__ __volatile__("fxrstor (%0)"
			:: "r" (ALIGN(current_task->fpu_save_data, 16)));

}

__attribute__((always_inline)) inline static void context_switch(task_t *new)
{
	asm("         \
	mov %1, %%esp;       \
	mov %2, %%ebp;       \
	mov %3, %%cr3; nop;"
	: : "r"(0), "r"(new->esp), "r"(new->ebp), 
		"r"(new->pd[1023]&PAGE_MASK) : "eax");
	
	asm("mov %0, %%ebx" :: "r"(current_task->preserved[0]));
	asm("mov %0, %%edi" :: "r"(current_task->preserved[1]));
	asm("mov %0, %%esi" :: "r"(current_task->preserved[2]));
}

__attribute__((always_inline)) 
inline static int engage_new_stack(task_t *task, task_t *parent)
{
	assert(parent == current_task);
	u32int ebp;
	u32int esp;
	asm("mov %%esp, %0" : "=r"(esp));
	asm("mov %%ebp, %0" : "=r"(ebp));
	if(esp > TOP_TASK_MEM) {
		task->esp=(esp-parent->kernel_stack) + task->kernel_stack;
		task->ebp=(ebp-parent->kernel_stack) + task->kernel_stack;
		task->sysregs = (parent->sysregs - parent->kernel_stack) + task->kernel_stack;
		cpu_copy_fixup_stack(task->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		task->sysregs = parent->sysregs;
		task->esp=esp;
		task->ebp=ebp;
		return 0;
	}
}

#endif
