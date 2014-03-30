#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
void arch_tm_set_current_task_marker(page_dir_t *space, addr_t task)
{
	space[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)task;
}
