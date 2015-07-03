#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/tm/context.h>
void arch_tm_set_current_thread_marker(page_dir_t *space, addr_t thread)
{
	space[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)thread;
}

void arch_tm_set_kernel_stack(addr_t start, addr_t end)
{
	set_kernel_stack(current_tss, end);
}

