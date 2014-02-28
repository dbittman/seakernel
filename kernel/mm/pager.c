#include <kernel.h>
#include <memory.h>
#include <task.h>
addr_t tmp_page;
void __KT_pager()
{
	tmp_page = pm_alloc_page();
	for(;;) {
#if CONFIG_SWAP
		__KT_swapper();
#else
		tm_process_pause((task_t *)current_task);
#endif
	}
}
