/* read_write.c: Copyright (c) 2010 Daniel Bittman
 * Functions for reading and writing files */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <sys/fcntl.h>
#include <char.h>
#include <block.h>
#include <file.h>

int do_sys_read_flags(struct file *f, off_t off, char *buf, size_t count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;
	int mode = inode->mode;
	if(S_ISFIFO(mode))
		return dm_read_pipe(inode, buf, count);
	else if(S_ISCHR(mode))
		return dm_char_rw(READ, inode->dev, buf, count);
	else if(S_ISBLK(mode))
		return dm_block_device_rw(READ, inode->dev, off, buf, count);
	/* We read the data for a link as well. If we have gotten to the point
	 * where we have the inode for the link we probably want to read the link 
	 * itself */
	else if(S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode))
		return read_fs(inode, off, count, buf);
	printk(1, "sys_read (%s): invalid mode %x\n", inode->name, inode->mode);
	return -EINVAL;
}

int do_sys_read(struct file *f, off_t off, char *buf, size_t count)
{
	int ret = do_sys_read_flags(f, off, buf, count);
	if(ret < 0) return ret;
	f->pos = off+ret;
	return ret;
}

int sys_read(int fp, off_t off, char *buf, size_t count)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	int ret = do_sys_read(f, off, buf, count);
	if(f) fput((task_t *)current_task, fp, 0);
	return ret;
}

int sys_readpos(int fp, char *buf, size_t count)
{
	if(!buf) 
		return -EINVAL;
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	if(!(f->flags & _FREAD)) {
		fput((task_t *)current_task, fp, 0);
		return -EACCES;
	}
	int ret = do_sys_read(f, f->pos, buf, count);
	fput((task_t *)current_task, fp, 0);
	return ret;
}

int do_sys_write_flags(struct file *f, off_t off, char *buf, size_t count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;
	if(S_ISFIFO(inode->mode))
		return dm_write_pipe(inode, buf, count);
	else if(S_ISCHR(inode->mode))
		return dm_char_rw(WRITE, inode->dev, buf, count);
	else if(S_ISBLK(inode->mode))
		return (dm_block_device_rw(WRITE, inode->dev, off, buf, count));
	/* Again, we want to write to the link because we have that node */
	else if(S_ISDIR(inode->mode) || S_ISREG(inode->mode) || S_ISLNK(inode->mode))
		return write_fs(inode, off, count, buf);
	printk(1, "sys_write (%s): invalid mode %x\n", inode->name, inode->mode);
	return -EINVAL;
}

int do_sys_write(struct file *f, off_t off, char *buf, size_t count)
{
	return do_sys_write_flags(f, off, buf, count);
}

int sys_writepos(int fp, char *buf, size_t count)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	if(!count || !buf) {
		fput((task_t *)current_task, fp, 0);
		return -EINVAL;
	}
	if(!(f->flags & _FWRITE)) {
		fput((task_t *)current_task, fp, 0);
		return -EACCES;
	}
	assert(f->inode);
	int ret=do_sys_write(f, f->flags & _FAPPEND ? f->inode->len : f->pos, buf, count);
	if(ret > 0)
		f->pos += ret;
	fput((task_t *)current_task, fp, 0);
	return ret;
}

int sys_write(int fp, off_t off, char *buf, size_t count)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	int ret = do_sys_write(f, off, buf, count);
	fput((task_t *)current_task, fp, 0);
	return ret;
}

int read_data(int fp, char *buf, off_t off, size_t length)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	int ret = do_sys_read_flags(f, off, buf, length);
	fput(current_task, fp, 0);
	return ret;
}
