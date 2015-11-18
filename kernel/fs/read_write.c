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
#include <sea/fs/dir.h>

ssize_t fs_file_pread(struct file *file, off_t offset, uint8_t *buffer, size_t length)
{
	ssize_t ret = -EIO;
	if(file->inode->kdev) {
		if(file->inode->kdev->rw)
			ret = file->inode->kdev->rw(READ, file, offset, buffer, length);
	} else {
		ret = fs_inode_read(file->inode, offset, length, buffer);
	}
	return ret;
}

ssize_t fs_file_read(struct file *file, uint8_t *buffer, size_t length)
{
	ssize_t ret = fs_file_pread(file, file->pos, buffer, length);
	if(ret > 0)
		file->pos += ret;
	return ret;
}

ssize_t fs_file_pwrite(struct file *file, off_t offset, uint8_t *buffer, size_t length)
{
	ssize_t ret = -EIO;
	if(file->inode->kdev) {
		if(file->inode->kdev->rw)
			ret = file->inode->kdev->rw(WRITE, file, offset, buffer, length);
	} else {
		ret = fs_inode_write(file->inode, offset, length, buffer);
	}
	return ret;
}

ssize_t fs_file_write(struct file *file, uint8_t *buffer, size_t length)
{
	ssize_t ret = fs_file_pwrite(file, file->pos, buffer, length);
	if(ret > 0)
		file->pos += ret;
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
	int ret = fs_file_read(f, buf, count);
	file_put(f);
	return ret;
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
	if(f->flags & _FAPPEND)
		f->pos = f->inode->length;
	int ret = fs_file_write(f, buf, count);
	file_put(f);
	return ret;
}

int sys_read(int fp, off_t pos, char *buf, size_t count)
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
	int ret = fs_file_pread(f, pos, buf, count);
	file_put(f);
	return ret;
}

int sys_write(int fp, off_t pos, char *buf, size_t count)
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
	int ret = fs_file_pwrite(f, pos, buf, count);
	file_put(f);
	return ret;
}

