#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
/* this is an extremely complicated function, so take some care
 * when reading it to increase understanding */
int __KT_pager(struct kthread *kt, void *arg)
{
	while(!kthread_is_joining(kt)){
		tm_process_pause((task_t *)current_task);
	}
	return 0;
}

