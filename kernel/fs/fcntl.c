/* fcntl.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for controlling and sending commands to files 
 */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <fcntl.h>
#include <file.h>

int sys_ioctl(int fp, int cmd, long arg)
{
	struct file *f = fs_get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	assert(f->inode);
	int ret = dm_ioctl(f->inode->mode, f->inode->dev, cmd, arg);
	fs_fput((task_t *)current_task, fp, 0);
	return ret;
}

int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3)
{
	int ret = 0;
	struct file *f = fs_get_file_pointer((task_t *)current_task, filedes);
	if(!f)
		return -EBADF;
	switch(cmd)
	{
		case F_DUPFD:
			ret = sys_dup2(filedes, attr1);
			break;
		case F_GETFD:
			ret = f->fd_flags;
			break;
		case F_SETFD:
			f->fd_flags = attr1;
			break;
		case F_GETFL:
			ret = f->flags;
			break;
		case F_SETFL:
			f->flags = attr1;
			break;
		case F_SETOWN: case F_GETOWN:
			printk(5, "Task attempted to access socket controls on non-socket descriptor!\n");
			tm_kill_process(current_task->pid);
			break;
		case F_SETLK: 
			ret = fs_fcntl_setlk(f, attr1);
			break;
		case F_GETLK: 
			ret = fs_fcntl_getlk(f, attr1);
			break;
		case F_SETLKW:
			ret = fs_fcntl_setlkw(f, attr1);
			break;
		default:
			printk(5, "Task tried calling fcntl with invalid commands!\n");
			ret = -EINVAL;
			break;
	}
	fs_fput((task_t *)current_task, filedes, 0);
	return ret;
}
