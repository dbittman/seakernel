#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/tables-x86_common.h>
#include <sea/cpu/processor.h>

void arch_tm_set_kernel_stack(struct cpu *cpu, addr_t start, addr_t end)
{
	/* TODO: is this thread safe? We're calling this during a context switch, so
	 * is preempt already off? is this gonna fuck that up? */
	set_kernel_stack(&cpu->arch_cpu_data.tss, end);
}

/* TODO: do we need to pass in flags? */
__attribute__((const)) const struct thread *arch_tm_get_current_thread(int flags)
{
	if(!(flags & KSF_THREADING))
		return 0;
	register uint32_t stack __asm__("esp");
	return *(struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
}

void arch_tm_jump_to_user_mode(addr_t jmp)
{
	/* TODO: solidify address space mappings, and stack stuff (locations, multiple stacks, etc) */
	asm("cli;"
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

