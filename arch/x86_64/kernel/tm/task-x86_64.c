#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/tm/thread.h>
void arch_cpu_set_kernel_stack(struct cpu *cpu, addr_t start, addr_t end)
{
	set_kernel_stack(&cpu->arch_cpu_data.tss, end);
}

__attribute__((const)) const struct thread **arch_tm_get_current_thread(void)
{
	register uint64_t stack __asm__("rsp");
	return (const struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
}

void arch_tm_jump_to_user_mode(addr_t jmp)
{
	__asm__ __volatile__ ("cli;"
			"movq %1, %%rbp;"
			"mov $0x23, %%ax;"
			"mov %%ax, %%ds;"
			"mov %%ax, %%es;"
			"mov %%ax, %%fs;"
			"pushq $0x23;"
			"pushq %1;"
			"pushfq;"
			"popq %%rax;"
			"orq $0x200, %%rax;"
			"pushq %%rax;"
			"pushq $0x1b;"
			"pushq %0;"
			"iretq;"
			::"r"(jmp), "r"(current_thread->usermode_stack_end):"memory","rax","rsp");
}

void arch_tm_do_switch(addr_t *, addr_t *, addr_t, addr_t);

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

