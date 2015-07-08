#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/tables-x86_common.h>
#include <sea/cpu/processor.h>

void arch_tm_set_kernel_stack(addr_t start, addr_t end)
{
	/* TODO: is this thread safe? We're calling this during a context switch, so
	 * is preempt already off? is this gonna fuck that up? */
	struct cpu *current_cpu = cpu_get_current();
	set_kernel_stack(&current_cpu->arch_cpu_data.tss, end);
	cpu_put_current(current_cpu);
}

/* TODO: do we need to pass in flags? */
struct thread *arch_tm_get_current_thread(int flags)
{
	if(!(flags & KSF_THREADING))
		return 0;
	register uint32_t stack __asm__("esp");
	return *(struct thread **)(stack & ~(KERN_STACK_SIZE - 1));
}

