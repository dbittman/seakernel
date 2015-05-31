#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/mm/reclaim.h>
#include <sea/vsprintf.h>
/* this is an extremely complicated function, so take some care
 * when reading it to increase understanding */
int slab_get_usage(void);
int __KT_pager(struct kthread *kt, void *arg)
{
	/* TODO: Need a good, clean API for this */
	current_task->priority = 10000;
	int active = 0;
	while(!kthread_is_joining(kt)) {
		/* reclaim memory if needed */
		int pm_use = (pm_used_pages * 100) / pm_num_pages;
		int km_use = slab_get_usage();
		if(pm_use > 50 || km_use > 50) {
			if(!mm_reclaim_size(PAGE_SIZE)) {
				tm_delay(500);
			} else {
				if(!active++)
					printk(0, "[mm]: activating memory reclaimer\n");
			}
		} else {
			/* TODO: Need a good clean API for this */
			tm_delay(1000);
			if(active > 0) active /= 2;
		}
	}
	return 0;
}

