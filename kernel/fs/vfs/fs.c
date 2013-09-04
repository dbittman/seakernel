#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

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
	return cget_idir(path, 0, 0x1FF);
}
