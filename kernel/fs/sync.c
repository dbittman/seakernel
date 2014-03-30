/* sync.c - Synchronization
 * copyright (c) Daniel Bittman 2012 */
#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/lib/cache.h>
#include <sea/dm/dev.h>
#include <sea/fs/mount.h>

/* This syncs things in order. That is, the block level cache syncs before 
 * the devices do because the block cache may modify the device cache */
int sys_sync(int disp)
{
	if(disp == -1)
		disp = PRINT_LEVEL;
	fs_do_sync_of_mounted();
	cache_sync_all();
	dm_sync();
	return 0;
}
