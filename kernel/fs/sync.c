/* sync.c - Synchronization
 * copyright (c) Daniel Bittman 2012 */
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
	fs_icache_sync();
	buffer_sync_all_dirty();
	dm_sync();
	return 0;
}

