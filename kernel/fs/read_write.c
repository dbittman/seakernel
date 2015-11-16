/* read_write.c: Copyright (c) 2010 Daniel Bittman
 * Functions for reading and writing files */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/sys/fcntl.h>
#include <sea/dm/char.h>
#include <sea/dm/block.h>
#include <sea/fs/file.h>
#include <sea/fs/pipe.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/dm/pty.h>

int fs_do_sys_read_flags(struct file *f, off_t off, char *buf, size_t count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;
	int mode = inode->mode;
	if(f->inode->kdev.rw) {
		return f->inode->kdev.rw(READ, f, off, buf, count);
	}
	if(S_ISFIFO(mode)) {
		return fs_pipe_read(inode, f->flags, buf, count);
	} else if(inode->pty)
		return pty_read(inode, buf, count);
	else if(S_ISCHR(mode))
		return dm_char_rw(READ, f, off, buf, count);
	else if(S_ISBLK(mode))
		return dm_block_device_rw(READ, inode->phys_dev, off, buf, count);
	/* We read the data for a link as well. If we have gotten to the point
	 * where we have the inode for the link we probably want to read the link 
	 * itself */
	else if(S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode)) {
		return fs_inode_read(inode, off, count, buf);
	}
	return -EINVAL;
}

int fs_do_sys_read(struct file *f, off_t off, char *buf, size_t count)
{
	int ret = fs_do_sys_read_flags(f, off, buf, count);
	if(ret < 0) return ret;
	f->pos = off+ret;
	return ret;
}

int sys_read(int fp, off_t off, char *buf, size_t count)
{
	struct file *f = file_get(fp);
	int ret = fs_do_sys_read(f, off, buf, count);
	if(f) file_put(f);
	return ret;
}

int sys_readpos(int fp, char *buf, size_t count)
{
	if(!buf) 
		return -EINVAL;
	struct file *f = file_get(fp);
	if(!f)
		return -EBADF;
	if(!(f->flags & _FREAD)) {
		file_put(f);
		return -EACCES;
	}
	int ret = fs_do_sys_read(f, f->pos, buf, count);
	file_put(f);
	return ret;
}

int fs_do_sys_write_flags(struct file *f, off_t off, char *buf, size_t count)
{
	if(!f || !buf)
		return -EINVAL;
	struct inode *inode = f->inode;

	if(f->inode->kdev.rw) {
		return f->inode->kdev.rw(WRITE, f, off, buf, count);
	}
	if(S_ISFIFO(inode->mode))
		return fs_pipe_write(inode, f->flags, buf, count);
	else if(inode->pty)
		return pty_write(inode, buf, count);
	else if(S_ISCHR(inode->mode))
		return dm_char_rw(WRITE, f, off, buf, count);
	else if(S_ISBLK(inode->mode))
		return (dm_block_device_rw(WRITE, inode->phys_dev, off, buf, count));
	/* Again, we want to write to the link because we have that node */
	else if(S_ISDIR(inode->mode) || S_ISREG(inode->mode) || S_ISLNK(inode->mode))
		return fs_inode_write(inode, off, count, buf);
	return -EINVAL;
}

int fs_do_sys_write(struct file *f, off_t off, char *buf, size_t count)
{
	return fs_do_sys_write_flags(f, off, buf, count);
}

int sys_writepos(int fp, char *buf, size_t count)
{
	struct file *f = file_get(fp);
	if(!f)
		return -EBADF;
	if(!count || !buf) {
		file_put(f);
		return -EINVAL;
	}
	if(!(f->flags & _FWRITE)) {
		file_put(f);
		return -EACCES;
	}
	assert(f->inode);
	if(f->flags & _FAPPEND)
		f->pos = f->inode->length;
	int ret=fs_do_sys_write(f, f->pos, buf, count);
	if(ret > 0)
		f->pos += ret;
	file_put(f);
	return ret;
}

int sys_write(int fp, off_t off, char *buf, size_t count)
{
	struct file *f = file_get(fp);
	if(!f)
		return -EBADF;
	int ret = fs_do_sys_write(f, off, buf, count);
	file_put(f);
	return ret;
}

int fs_read_file_data(int fp, char *buf, off_t off, size_t length)
{
	struct file *f = file_get(fp);
	if(!f) return -EBADF;
	int ret = fs_do_sys_read_flags(f, off, buf, length);
	file_put(f);
	return ret;
}

