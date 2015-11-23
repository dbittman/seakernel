#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/mm/reclaim.h>
#include <sea/vsprintf.h>
#include <sea/tm/timing.h>
/* this is an extremely complicated function, so take some care
 * when reading it to increase understanding */
int slab_get_usage(void);
int __KT_pager(struct kthread *kt, void *arg)
{
	/* TODO: Need a good, clean API for this */
	current_thread->priority = 10000;
	int active = 0;
	while(!kthread_is_joining(kt)) {
		/* reclaim memory if needed */
		int km_use = slab_get_usage();
		int pm_use = mm_physical_get_usage();
		if(km_use > 50 || pm_use > 50) {
			if(!mm_reclaim_size(128)) {
				tm_thread_delay(ONE_SECOND / 2);
			} else {
				if(!active++)
					printk(0, "[mm]: activating memory reclaimer\n");
			}
		} else {
			tm_thread_delay(ONE_SECOND);
			if(active > 0) active /= 2;
		}
	}
	return 0;
}

