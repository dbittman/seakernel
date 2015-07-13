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
__attribute__((const)) const struct thread *arch_tm_get_current_thread(int flags)
{
	if(!(flags & KSF_THREADING))
		return 0;
	register uint32_t stack __asm__("esp");
	return *(struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
}

void arch_tm_jump_to_user_mode(addr_t jmp)
{
	__asm__ __volatile__ ("cli;"
			"xchg %%bx, %%bx;"
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

__attribute__((noinline)) void arch_tm_thread_switch(struct thread *old, struct thread *new)
{
	/* TODO fpu, sse */
	assert(new->stack_pointer > (addr_t)new->kernel_stack + sizeof(addr_t));
	cpu_set_kernel_stack(new->cpu, (addr_t)new->kernel_stack,
			(addr_t)new->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	if(new->process != old->process) {
		mm_vm_switch_context(&new->process->vmm_context);
	}
	__asm__ __volatile__ (
		"pushf;"
		"push %%ebx;"
		"push %%esi;"
		"push %%edi;"
		"push %%ebp;"
		"mov %%esp, %0;": "=r"(old->stack_pointer)::"memory");
	if(new->jump_point) {
		/* newly created thread, needs to just have some basic context set
		 * up initially and then jumped to */
		addr_t jump = new->jump_point;
		new->jump_point = 0;
		__asm__ __volatile__ ("mov %1, %%ecx;"
				"mov %0, %%esp;"
				"pop %%ebp;"
				"jmp *%%ecx"::"r"(new->stack_pointer), "r"(jump):"memory");
	}
	__asm__ __volatile__ ("mov %0, %%esp;"
			"pop %%ebp;"
			"pop %%edi;"
			"pop %%esi;"
			"pop %%ebx;"
			"popf"::"r"(new->stack_pointer):"memory");
	/* WARNING - we've switched stacks at this point! We must NOT use anything
	 * stack related now until this function returns! */
}

__attribute__((noinline)) void arch_tm_fork_setup_stack(struct thread *thr)
{
	arch_cpu_copy_fixup_stack((addr_t)thr->kernel_stack, (addr_t)current_thread->kernel_stack, KERN_STACK_SIZE);
	*(struct thread **)(thr->kernel_stack) = thr;
	addr_t esp;
	addr_t ebp;
	__asm__ __volatile__ ("mov %%esp, %0":"=r"(esp));
	__asm__ __volatile__ ("mov %%ebp, %0":"=r"(ebp));

	esp -= (addr_t)current_thread->kernel_stack;
	ebp -= (addr_t)current_thread->kernel_stack;
	esp += (addr_t)thr->kernel_stack;
	ebp += (addr_t)thr->kernel_stack;

	esp += 4;
	*(addr_t *)esp = ebp;
	thr->stack_pointer = esp;
	thr->jump_point = (addr_t)arch_tm_read_ip();
}

