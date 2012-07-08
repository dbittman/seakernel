/* read_write.c: Copyright (c) 2010 Daniel Bittman
 * Functions for reading and writing files */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <sys/fcntl.h>

int do_sys_read_flags(struct file *f, unsigned off, char *buf, unsigned count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;
	int mode = inode->mode;
	if(S_ISFIFO(mode))
		return read_pipe(inode, buf, count);
	else if(S_ISCHR(mode))
		return char_rw(READ, inode->dev, buf, count);
	else if(S_ISBLK(mode))
		return block_device_rw(READ, inode->dev, off, buf, count);
	/* We read the data for a link as well. If we have gotten to the point
	 * where we have the inode for the link we probably want to read the link 
	 * itself */
	else if(S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode))
		return read_fs(inode, off, count, buf);
	printk(1, "sys_read (%s): invalid mode %x\n", inode->name, inode->mode);
	return -EINVAL;
}

int do_sys_read(struct file *f, unsigned off, char *buf, unsigned count)
{
	int ret = do_sys_read_flags(f, off, buf, count);
	if(ret < 0) return ret;
	f->pos = off+ret;
	return ret;
}

int sys_read(int fp, unsigned off, char *buf, unsigned count)
{
	return do_sys_read(get_file_pointer((task_t *)current_task, fp), off, buf, count);
}

int sys_readpos(int fp, char *buf, unsigned count)
{
	if(!buf) 
		return -EINVAL;
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	if(!(f->flags & _FREAD))
		return -EACCES;
	return do_sys_read(f, f->pos, buf, count);
}

int do_sys_write_flags(struct file *f, unsigned off, char *buf, unsigned count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;
	if(S_ISFIFO(inode->mode))
		return write_pipe(inode, buf, count);
	else if(S_ISCHR(inode->mode))
		return char_rw(WRITE, inode->dev, buf, count);
	else if(S_ISBLK(inode->mode))
		return (block_device_rw(WRITE, inode->dev, off, buf, count));
	/* Again, we want to write to the link because we have that node */
	else if(S_ISDIR(inode->mode) || S_ISREG(inode->mode) || S_ISLNK(inode->mode))
		return write_fs(inode, off, count, buf);
	printk(1, "sys_write (%s): invalid mode %x\n", inode->name, inode->mode);
	return -EINVAL;
}

int do_sys_write(struct file *f, unsigned off, char *buf, unsigned count)
{
	return do_sys_write_flags(f, off, buf, count);
}

int sys_writepos(int fp, char *buf, unsigned count)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	if(!count || !buf)
		return -EINVAL;
	if(!(f->flags & _FWRITE))
		return -EACCES;
	int pos = f->pos;
	assert(f->inode);
	if(f->flags & _FAPPEND) pos = f->inode->len;
	pos=do_sys_write(f, pos, buf, count);
	if(pos > 0)
		f->pos += pos;
	return pos;
}

int sys_write(int fp, unsigned off, char *buf, unsigned count)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	return do_sys_write(f, off, buf, count);
}
