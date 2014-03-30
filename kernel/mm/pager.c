#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
addr_t tmp_page;
void __KT_pager()
{
	tmp_page = mm_alloc_physical_page();
	for(;;) {
#if CONFIG_SWAP
		__KT_swapper();
#else
		tm_process_pause((task_t *)current_task);
#endif
	}
}
