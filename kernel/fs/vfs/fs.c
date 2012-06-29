#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
extern struct inode *devfs_root, *procfs_root;

int sync_inode_tofs(struct inode *i)
{
	if(!i)
		return -EINVAL;
	return vfs_callback_sync_inode(i);
}

struct inode *sys_create(char *path)
{
	return cget_idir(path, 0, current_task->cmask);
}
