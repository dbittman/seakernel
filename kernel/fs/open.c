/* open.c: Copyright (c) 2010 Daniel Bittman
 * Functions for gaining access to a file (sys_open, sys_getidir, duplicate)
 */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <sys/fcntl.h>

struct file *d_sys_open(char *name, int flags, int mode, int *error, int *num)
{
	if(!name) {
		*error = -EINVAL;
		return 0;
	}
	++flags;
	struct inode *inode;
	struct file *f;
	if(!mode)
		mode = current_task->cmask;
	else if(current_task->cmask)
		mode &= current_task->cmask;
	if(!mode) mode = 0x1FF;
	int did_create=0;
	inode = (flags & _FCREAT) ? 
				ctget_idir(name, 0, mode, &did_create) 
				: get_idir(name, 0);
	if(!inode) {
		*error = (flags & _FCREAT) ? -EACCES : -ENOENT;
		return 0;
	} else {
		/* If CREAT and EXCL are set, and the file exists, return */
		if(flags & _FCREAT && flags & _FEXCL && !did_create) {
			iput(inode);
			*error = -EEXIST;
			return 0;
		}
	}
	if(flags & _FREAD && !permissions(inode, MAY_READ)) {
		iput(inode);
		*error = -EACCES;
		return 0;
	}
	if(flags & _FWRITE && !permissions(inode, MAY_WRITE)) {
		iput(inode);
		*error = -EACCES;
		return 0;
	}
	int ret;
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flags = flags;
	f->pos=0;
	f->count=1;
	f->fd_flags &= ~FD_CLOEXEC;
	mutex_on(&inode->lock);
	inode->f_count++;
	mutex_off(&inode->lock);
	ret = add_file_pointer((task_t *)current_task, f);
	if(num) *num = ret;
	if(S_ISCHR(inode->mode) && !(flags & _FNOCTTY))
		char_rw(OPEN, inode->dev, 0, 0);
	if(flags & _FTRUNC && !is_directory(inode))
	{
		mutex_on(&inode->lock);
		inode->len=0;
		sync_inode_tofs(inode);
		mutex_off(&inode->lock);
	}
	return f;
}

int sys_open_posix(char *name, int flags, int mode)
{
	int error=0, num;
	struct file *f = d_sys_open(name, flags, mode, &error, &num);
	if(!f)
		return error;
	return num;
}

int sys_open(char *name, int flags)
{
	return sys_open_posix(name, flags, 0);
}

int duplicate(task_t *t, int fp, int n)
{
	struct file *f = get_file_pointer(t, fp);
	if(!f)
		return -EBADF;
	struct file *new=(struct file *)kmalloc(sizeof(struct file));
	new->inode = f->inode;
	assert(new->inode && new->inode->count && new->inode->f_count && !new->inode->unreal);
	new->count=1;
	mutex_on(&f->inode->lock);
	f->inode->count++;
	f->inode->f_count++;
	mutex_off(&f->inode->lock);
	new->flags = f->flags;
	new->fd_flags = f->fd_flags;
	new->fd_flags &= ~FD_CLOEXEC;
	new->pos = f->pos;
	if(f->inode->pipe && !f->inode->pipe->type) {
		mutex_on(f->inode->pipe->lock);
		++f->inode->pipe->count;
		if(f->flags & _FWRITE) f->inode->pipe->wrcount++;
		mutex_off(f->inode->pipe->lock);
	}
	int ret = 0;
	if(n)
		ret = add_file_pointer_after(t, new, n);
	else
		ret = add_file_pointer(t, new);
	return ret;
}

int sys_dup(int f)
{
	return duplicate((task_t *)current_task, f, 0);
}

int sys_dup2(int f, int n)
{
	return duplicate((task_t *)current_task, f, n);
}
