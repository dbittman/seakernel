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

