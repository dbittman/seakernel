#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/tm/thread.h>
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
__attribute__((const)) const struct thread *arch_tm_get_current_thread(int flags)
{
	if(!(flags & KSF_THREADING))
		return 0;
	register uint64_t stack __asm__("rsp");
	if((stack & 0xFFF) <= 0x10) {
		panic(PANIC_NOSYNC, "kernel stack in danger of overrunning!");
	}
	return *(struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
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

__attribute__((noinline)) void arch_tm_thread_switch(struct thread *old, struct thread *new)
{
	assert(old != new);
	addr_t jump = new->jump_point;
	new->jump_point = 0;
	if(!(new->stack_pointer > (addr_t)new->kernel_stack + sizeof(addr_t))) {
		panic(0, "kernel stack overrun! thread=%x:%d %x (min = %x)", new, new->tid, new->stack_pointer, new->kernel_stack);
	}
	cpu_set_kernel_stack(new->cpu, (addr_t)new->kernel_stack,
			(addr_t)new->kernel_stack + (KERN_STACK_SIZE));
	if(new->process != old->process) {
		mm_vm_switch_context(&new->process->vmm_context);
	}
	/* TODO: determine when to actually do this. It's a waste of time to do it for every thread */
	__asm__ __volatile__ (
			"fxsave (%0)" :: "r" (ALIGN(old->arch_thread.fpu_save_data, 16)) : "memory");
	if(!jump) {
		__asm__ __volatile__ (
				"fxrstor (%0)" :: "r" (ALIGN(new->arch_thread.fpu_save_data, 16)) : "memory");
	}
	if(jump)
		__asm__("xchg %%bx, %%bx" :);
	arch_tm_do_switch(&old->stack_pointer, &new->stack_pointer, jump);
	/* WARNING - we've switched stacks at this point! We must NOT use anything
	 * stack related now until this function returns! */
}

__attribute__((noinline)) void arch_tm_fork_setup_stack(struct thread *thr)
{
	arch_cpu_copy_fixup_stack((addr_t)thr->kernel_stack, (addr_t)current_thread->kernel_stack, KERN_STACK_SIZE);
	*(struct thread **)(thr->kernel_stack) = thr;

	arch_tm_do_fork_setup(&thr->stack_pointer, &thr->jump_point, (signed long)((addr_t)thr->kernel_stack - (addr_t)current_thread->kernel_stack));
}

