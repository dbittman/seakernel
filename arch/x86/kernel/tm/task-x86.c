#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/tables-x86_common.h>
#include <sea/cpu/processor.h>
void arch_tm_set_current_thread_marker(page_dir_t *space, addr_t thread)
{
	space[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)thread;
}

void arch_tm_set_kernel_stack(addr_t start, addr_t end)
{
	/* TODO: is this thread safe? We're calling this during a context switch, so
	 * is preempt already off? is this gonna fuck that up? */
	struct cpu *current_cpu = cpu_get_current();
	set_kernel_stack(&current_cpu->arch_cpu_data.tss, end);
	cpu_put_current(current_cpu);
}

