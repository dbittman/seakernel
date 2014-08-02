#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
/* this is an extremely complicated function, so take some care
 * when reading it to increase understanding */
void __KT_pager()
{
	for(;;) {
		tm_process_pause((task_t *)current_task);
	}
}

