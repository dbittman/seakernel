#include <kernel.h>
#include <task.h>

void arch_specific_set_current_task(page_dir_t *space, addr_t task)
{
	space[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)task;
}
