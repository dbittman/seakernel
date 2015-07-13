#ifndef __CONTEXT_X86_64_H
#define __CONTEXT_X86_64_H
/* Functions for scheduling tasks */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>

#define current_tss (&(current_task->cpu->arch_cpu_data.tss))

static void _overflow(char *type)
{
	printk(5, "%s overflow occurred in task %d (esp=%x, ebp=%x, heap_end=%x). Killing...\n", 
		   type, current_task->pid, current_task->esp, current_task->ebp, 
		current_task->heap_end);
	tm_process_suicide();
}

__attribute__((always_inline)) inline static void store_context_fork(task_t *task)
{
	asm("mov %%rbx, %0" : "=r"(task->preserved[0]));
	asm("mov %%r12, %0" : "=r"(task->preserved[1]));
	asm("mov %%r13, %0" : "=r"(task->preserved[2]));
	asm("mov %%r14, %0" : "=r"(task->preserved[3]));
	asm("mov %%r15, %0" : "=r"(task->preserved[4]));
}

__attribute__((always_inline)) inline static void store_context(void)
{
	asm("mov %%rsp, %0" : "=r"(current_task->esp));
	asm("mov %%rbp, %0" : "=r"(current_task->ebp));
	/* we save all registers, in case schedule was called within kernel
	 * code, since that doesn't save registers like an interrupt */
	asm("mov %%rbx, %0" : "=r"(current_task->preserved[0]));
	asm("mov %%r12, %0" : "=r"(current_task->preserved[1]));
	asm("mov %%r13, %0" : "=r"(current_task->preserved[2]));
	asm("mov %%r14, %0" : "=r"(current_task->preserved[3]));
	asm("mov %%r15, %0" : "=r"(current_task->preserved[4]));
	/* TODO: There is a lot of overhead here, because we don't need
	 * to do this for every task. For now, this works, but it needs
	 * to be fixed. */
	if(current_task->cpu->flags & CPU_FXSAVE || current_task->cpu->flags & CPU_SSE || current_task->cpu->flags & CPU_FPU)
		__asm__ __volatile__("fxsave64 (%0)"
		:: "r" (ALIGN(current_task->fpu_save_data, 16)));
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
		tm_raise_flag(TF_BURIED);
	current_task->syscall_count = 0;
}

__attribute__((always_inline)) inline static void restore_context(task_t *n)
{
	/* Update some last-minute things. The stack. */
	set_kernel_stack(current_tss, (n->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE)) & ~0xF);
	if(n->cpu->flags & CPU_SSE || n->cpu->flags & CPU_FPU)
		__asm__ __volatile__("fxrstor64 (%0)"
		:: "r" (ALIGN(current_task->fpu_save_data, 16)));
}

__attribute__((always_inline)) inline static void context_switch(task_t *n)
{
	asm("         \
	mov %1, %%rsp;       \
	mov %2, %%rbp;       \
	mov %3, %%cr3; nop;"
	: : "r"(0), "r"(n->esp), "r"(n->ebp), 
		"r"(n->pd[PML4_IDX(PHYSICAL_PML4_INDEX/0x1000)]&PAGE_MASK) : "rax");
	
	asm("mov %0, %%rbx" :: "r"(current_task->preserved[0]));
	asm("mov %0, %%r12" :: "r"(current_task->preserved[1]));
	asm("mov %0, %%r13" :: "r"(current_task->preserved[2]));
	asm("mov %0, %%r14" :: "r"(current_task->preserved[3]));
	asm("mov %0, %%r15" :: "r"(current_task->preserved[4]));
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
		cpu_copy_fixup_stack(task->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		task->flags |= THREAD_FORK_COPIEDUSER;
		task->sysregs = parent->sysregs;
		task->esp=esp;
		task->ebp=ebp;
		return 0;
	}
}

#endif
