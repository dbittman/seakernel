#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>

void __KT_pager()
{
	for(;;) {
		tm_process_pause((task_t *)current_task);
	}
}
