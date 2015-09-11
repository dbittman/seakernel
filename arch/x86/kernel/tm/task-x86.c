#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/tables-x86_common.h>
#include <sea/cpu/processor.h>

void arch_cpu_set_kernel_stack(struct cpu *cpu, addr_t start, addr_t end)
{
	set_kernel_stack(&cpu->arch_cpu_data.tss, end);
}

/* we need to pass in flags to this because the value returned will change
 * based on kernel state: specifically, if threading is on or not. Once the
 * threading system gets initialized, this function will need to return the
 * current thread, but before that, it will need to return 0. In order to
 * be able to define it as constant for optimization reasons, we need to
 * pass in the kernel state flags so that the optimizer knows. */
__attribute__((const)) const struct thread **arch_tm_get_current_thread(void)
{
	register uint32_t stack __asm__("esp");
	return (const struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
}

void arch_tm_jump_to_user_mode(addr_t jmp)
{
	__asm__ __volatile__ ("cli;"
			"mov %1, %%ebp;"
			"mov $0x23, %%ax;"
			"mov %%ax, %%ds;"
			"mov %%ax, %%es;"
			"mov %%ax, %%fs;"
			"push $0x23;"
			"push %1;"
			"pushf;"
			"pop %%eax;"
			"or $0x200, %%eax;"
			"push %%eax;"
			"push $0x1b;"
			"push %0;"
			"iret;"
			::"r"(jmp), "r"(current_thread->usermode_stack_end):"memory","eax","esp");
}

void arch_tm_do_switch(long unsigned *, long unsigned *, addr_t, addr_t);

__attribute__((noinline)) void arch_tm_thread_switch(struct thread *old, struct thread *new, addr_t jump)
{
	/* TODO: determine when to actually do this. It's a waste of time to do it for every thread */
	__asm__ __volatile__ (
			"fxsave (%0)" :: "r" (ALIGN(old->arch_thread.fpu_save_data, 16)) : "memory");
	if(!jump) {
		__asm__ __volatile__ (
				"fxrstor (%0)" :: "r" (ALIGN(new->arch_thread.fpu_save_data, 16)) : "memory");
	}
	addr_t cr3 = (old->process != new->process) ? new->process->vmm_context.root_physical : 0;
	if(jump)
		cpu_enable_preemption();
	arch_tm_do_switch(&old->stack_pointer, &new->stack_pointer, jump, cr3);
	/* WARNING - we've switched stacks at this point! We must NOT use anything
	 * stack related now until this function returns! */
}

void arch_tm_do_fork_setup(long unsigned *stack, long unsigned *jmp, signed long offset);

__attribute__((noinline)) void arch_tm_fork_setup_stack(struct thread *thr)
{
	arch_cpu_copy_fixup_stack((addr_t)thr->kernel_stack, (addr_t)current_thread->kernel_stack, KERN_STACK_SIZE);
	*(struct thread **)(thr->kernel_stack) = thr;
	arch_tm_do_fork_setup(&thr->stack_pointer, &thr->jump_point, (signed long)((addr_t)thr->kernel_stack - (addr_t)current_thread->kernel_stack));
}

