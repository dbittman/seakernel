/* open.c: Copyright (c) 2010 Daniel Bittman
 * Functions for gaining access to a file (sys_open, sys_getidir, duplicate)
 */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/sys/fcntl.h>
#include <sea/dm/char.h>
#include <sea/dm/block.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/file.h>
#include <sea/dm/pipe.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

struct file *fs_do_sys_open(char *name, int flags, mode_t _mode, int *error, int *num)
{
	if(!name) {
		*error = -EINVAL;
		return 0;
	}
	++flags;
	struct inode *inode;
	struct file *f;
	mode_t mode = (_mode & ~0xFFF) | ((_mode&0xFFF) & (~(current_task->cmask&0xFFF)));
	int did_create=0;
	inode = (flags & _FCREAT) ? 
				fs_resolve_path_create(name, 0, S_IFREG | mode, &did_create) 
				: fs_resolve_path_inode(name, 0, 0);
	if(!inode) {
		*error = (flags & _FCREAT) ? -EACCES : -ENOENT;
		return 0;
	}
	/* If CREAT and EXCL are set, and the file exists, return */
	if(flags & _FCREAT && flags & _FEXCL && !did_create) {
		vfs_icache_put(inode);
		*error = -EEXIST;
		return 0;
	}
	if(flags & _FREAD && !vfs_inode_check_permissions(inode, MAY_READ, 0)) {
		vfs_icache_put(inode);
		*error = -EACCES;
		return 0;
	}
	if(flags & _FWRITE && !vfs_inode_check_permissions(inode, MAY_WRITE, 0)) {
		vfs_icache_put(inode);
		*error = -EACCES;
		return 0;
	}
	if((flags & _FNONBLOCK) && (flags & _FWRITE) && S_ISFIFO(inode->mode) && inode->pipe) {
		/* check if we have readers on this fifo. If not, we return an error */
		if((inode->pipe->count - inode->pipe->wrcount) == 0) {
			vfs_icache_put(inode);
			*error = -ENXIO;
			return 0;
		}
	}
	int ret;
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->flags = flags;
	f->pos=0;
	f->count=1;
	f->fd_flags &= ~FD_CLOEXEC;
	ret = fs_add_file_pointer((task_t *)current_task, f);
	if(num) *num = ret;
	if(S_ISCHR(inode->mode) && !(flags & _FNOCTTY))
		dm_char_rw(OPEN, inode->phys_dev, 0, 0);
	if(flags & _FTRUNC && S_ISREG(inode->mode))
	{
		inode->length=0;
		inode->ctime = inode->mtime = time_get_epoch();
		vfs_inode_set_dirty(inode);
	}
	if(S_ISFIFO(inode->mode) && inode->pipe) {
		mutex_acquire(inode->pipe->lock);
		add_atomic(&inode->pipe->count, 1);
		mutex_release(inode->pipe->lock);
	}
	fs_fput((task_t *)current_task, ret, 0);
	return f;
}

int sys_open_posix(char *name, int flags, mode_t mode)
{
	int error=0, num;
	struct file *f = fs_do_sys_open(name, flags, mode, &error, &num);
	if(!f)
		return error;
	return num;
}

int sys_open(char *name, int flags)
{
	return sys_open_posix(name, flags, 0);
}

static int duplicate(task_t *t, int fp, int n)
{
	struct file *f = fs_get_file_pointer(t, fp);
	if(!f)
		return -EBADF;
	struct file *new=(struct file *)kmalloc(sizeof(struct file));
	new->inode = f->inode;
	new->count=1;
	add_atomic(&f->inode->count, 1);
	new->flags = f->flags;
	new->fd_flags = f->fd_flags;
	new->fd_flags &= ~FD_CLOEXEC;
	new->pos = f->pos;
	if(f->inode->pipe && !f->inode->pipe->type) {
		add_atomic(&f->inode->pipe->count, 1);
		if(f->flags & _FWRITE) add_atomic(&f->inode->pipe->wrcount, 1);
		tm_remove_all_from_blocklist(f->inode->pipe->read_blocked);
		tm_remove_all_from_blocklist(f->inode->pipe->write_blocked);
	}
	int ret = 0;
	if(n)
		ret = fs_add_file_pointer_after(t, new, n);
	else
		ret = fs_add_file_pointer(t, new);
	fs_fput((task_t *)t, fp, 0);
	fs_fput((task_t *)t, ret, 0);
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
