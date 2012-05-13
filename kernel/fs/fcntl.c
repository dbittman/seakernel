/* fcntl.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for controlling and sending commands to files 
 */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <fcntl.h>

int sys_ioctl(int fp, int cmd, int arg)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	assert(f->inode);
	return dm_ioctl(f->inode->mode, f->inode->dev, cmd, arg);
}

int sys_fcntl(int filedes, int cmd, int attr1, int attr2, int attr3)
{
	struct file *f = get_file_pointer((task_t *)current_task, filedes);
	if(!f)
		return -EBADF;
	switch(cmd)
	{
		case F_DUPFD:
			return sys_dup2(filedes, attr1);
			break;
		case F_GETFD:
			return f->fd_flags;
			break;
		case F_SETFD:
			f->fd_flags = attr1;
			break;
		case F_GETFL:
			return f->flags;
			break;
		case F_SETFL:
			f->flags = attr1;
			break;
		case F_SETOWN: case F_GETOWN:
			printk(5, "Task attempted to access socket controls on non-socket descriptor!\n");
			kill_task(current_task->pid);
			break;
		case F_SETLK: 
			return fcntl_setlk(f, attr1);
			break;
		case F_GETLK: 
			return fcntl_getlk(f, attr1);
			break;
		case F_SETLKW:
			return fcntl_setlkw(f, attr1);
			break;
		default:
			printk(5, "Task tried calling fcntl with invalid commands!\n");
			return -EINVAL;
			break;
	}
	return 0;
}
