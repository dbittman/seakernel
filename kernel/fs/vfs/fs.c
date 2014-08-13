#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
#include <sea/errno.h>
int sync_inode_tofs(struct inode *i)
{
	if(!i)
		return -EINVAL;
	rwlock_acquire(&i->rwl, RWL_WRITER);
	int r = vfs_callback_sync_inode(i);
	rwlock_release(&i->rwl, RWL_WRITER);
	return r;
}

struct inode *sys_create(char *path)
{
	return vfs_cget_idir(path, 0, 0x1FF);
}
