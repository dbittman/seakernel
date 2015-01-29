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

static struct inode *__fs_do_open_resolve__(char *path, int *err, struct dirent **dir)
{
	struct dirent *de = fs_path_resolve(path, 0, err);
	if(!de)
		return 0;
	*dir = de;
	struct inode *node = fs_dirent_readinode(de, 0);
	if(!node) {
		*err = -EIO;
	}
	return node;
}

struct file *fs_do_sys_open(char *name, int flags, mode_t _mode, int *error, int *num)
{
	if(!name) {
		*error = -EINVAL;
		return 0;
	}
	++flags;
	struct inode *inode=0;
	struct dirent *dirent=0;
	struct file *f;
	mode_t mode = (_mode & ~0xFFF) | ((_mode&0xFFF) & (~(current_task->cmask&0xFFF)));
	
	inode = (flags & _FCREAT) ?
				do_fs_path_resolve_create(name, 0, 
						S_IFREG | mode, error, &dirent)
				: __fs_do_open_resolve__(name, error, &dirent);

	if(!inode || !dirent) {
		if(inode) vfs_icache_put(inode);
		if(dirent) vfs_dirent_release(dirent);
		return 0;
	}
	/* If CREAT and EXCL are set, and the file exists, return */
	if(flags & _FCREAT && flags & _FEXCL && !*error) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		*error = -EEXIST;
		return 0;
	}
	if(flags & _FREAD && !vfs_inode_check_permissions(inode, MAY_READ, 0)) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		*error = -EACCES;
		return 0;
	}
	if(flags & _FWRITE && !vfs_inode_check_permissions(inode, MAY_WRITE, 0)) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		*error = -EACCES;
		return 0;
	}
	if((flags & _FNONBLOCK) && (flags & _FWRITE) && S_ISFIFO(inode->mode) && inode->pipe) {
		/* check if we have readers on this fifo. If not, we return an error */
		if((inode->pipe->count - inode->pipe->wrcount) == 0) {
			vfs_icache_put(inode);
			vfs_dirent_release(dirent);
			*error = -ENXIO;
			return 0;
		}
	}
	int ret;
	f = (struct file *)kmalloc(sizeof(struct file));
	f->inode = inode;
	f->dirent = dirent;
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
	vfs_inode_get(f->inode);
	if(f->dirent)
		vfs_dirent_acquire(f->dirent);
	new->inode = f->inode;
	new->dirent = f->dirent;
	new->count=1;
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
