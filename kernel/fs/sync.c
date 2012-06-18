/* sync.c - Synchronization
 * copyright (c) Daniel Bittman 2012 */
#include <kernel.h>
#include <fs.h>
#include <cache.h>

/* This syncs things in order. That is, the block level cache syncs before 
 * the devices do because the block cache may modify the device cache */
int sys_sync(int disp)
{
	if(disp == -1)
		disp = PRINT_LEVEL;
	mmf_sync();
	do_sync_of_mounted();
	kernel_cache_sync();
	sync_dm();
	return 0;
}
