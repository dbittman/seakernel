/* fcntl.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for controlling and sending commands to files 
 */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/fs/fcntl.h>
#include <sea/fs/file.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>

int sys_ioctl(int fp, int cmd, long arg)
{
	struct file *f = file_get(fp);
	if(!f) return -EBADF;
	assert(f->inode);
	int ret = 0;
	if(f->inode->pty)
		ret = pty_ioctl(f->inode, cmd, arg);
	//else
	//	ret = dm_ioctl(f->inode->mode, f->inode->phys_dev, cmd, arg);
	file_put(f);
	return ret;
}

int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3)
{
	int ret = 0;
	struct file *f = file_get(filedes);
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
			tm_signal_send_thread(current_thread, SIGABRT);
			break;
		default:
			printk(5, "Task tried calling fcntl with invalid commands!\n");
			ret = -EINVAL;
			break;
	}
	file_put(f);
	return ret;
}
