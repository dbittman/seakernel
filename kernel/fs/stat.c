/* kernel/fs/stat.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for gaining information about a file */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/sys/fcntl.h>
#include <sea/fs/file.h>
#include <sea/fs/dir.h>
#include <sea/fs/callback.h>
#include <sea/dm/pipe.h>
int sys_isatty(int f)
{
	struct file *file = fs_get_file_pointer((task_t *) current_task, f);
	if(!file) return -EBADF;
	struct inode *inode = file->inode;
	if(S_ISCHR(inode->mode) && (MAJOR(inode->dev) == 3 || MAJOR(inode->dev) == 4)) {
		fs_fput((task_t *)current_task, f, 0);
		return 1;
	}
	fs_fput((task_t *)current_task, f, 0);
	return 0;
}

int sys_getpath(int f, char *b, int len)
{
	if(!b) return -EINVAL;
	struct file *file = fs_get_file_pointer((task_t *) current_task, f);
	if(!file)
		return -EBADF;
	int ret = vfs_get_path_string(file->inode, b, len);
	fs_fput((task_t *)current_task, f, 0);
	return ret;
}

static void do_stat(struct inode * inode, struct stat * tmp)
{
	assert(inode && tmp);
	tmp->st_dev = inode->dev;
	tmp->st_ino = inode->num;
	tmp->st_mode = inode->mode;
	tmp->st_uid = inode->uid;
	tmp->st_gid = inode->gid;
	tmp->st_rdev = inode->dev;
	tmp->st_size = inode->len;
	tmp->st_blocks = inode->nblocks;
	tmp->st_nlink = inode->nlink;
	tmp->st_atime = inode->atime;
	tmp->st_mtime = inode->mtime;
	tmp->st_ctime = inode->ctime;
	tmp->st_blksize = inode->blksize;
	if(inode->pipe) {
		tmp->st_blksize = PAGE_SIZE;
		tmp->st_blocks = PIPE_SIZE / PAGE_SIZE;
		tmp->st_size = inode->pipe->pending;
	}
	/* HACK: some filesystems might not set this... */
	if(!tmp->st_blksize)
		tmp->st_blksize = 512;
}

int sys_stat(char *f, struct stat *statbuf, int lin)
{
	if(!f || !statbuf) return -EINVAL;
	struct inode *i;
	i = (struct inode *) (lin ? vfs_lget_idir(f, 0) : vfs_get_idir(f, 0));
	if(!i)
		return -ENOENT;
	do_stat(i, statbuf);
	vfs_iput(i);
	return 0;
}

int sys_dirstat(char *dir, unsigned num, char *namebuf, struct stat *statbuf)
{
	if(!namebuf || !statbuf || !dir)
		return -EINVAL;
	struct inode *i = vfs_read_dir(dir, num);
	if(!i)
		return -ESRCH;
	do_stat(i, statbuf);
	strncpy(namebuf, i->name, 128);
	vfs_iput(i);
	return 0;
}

int sys_dirstat_fd(int fd, unsigned num, char *namebuf, struct stat *statbuf)
{
	if(!namebuf || !statbuf)
		return -EINVAL;
	struct file *f = fs_get_file_pointer((task_t *)current_task, fd);
	if(!f) return -EBADF;
	struct inode *i = vfs_read_idir(f->inode, num);
	if(!i) {
		fs_fput((task_t *)current_task, fd, 0);
		return -ESRCH;
	}
	do_stat(i, statbuf);
	strncpy(namebuf, i->name, 128);
	vfs_iput(i);
	fs_fput((task_t *)current_task, fd, 0);
	return 0;
}

int sys_fstat(int fp, struct stat *sb)
{
	if(!sb)
		return -EINVAL;
	struct file *f = fs_get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	do_stat(f->inode, sb);
	fs_fput((task_t *)current_task, fp, 0);
	return 0;
}

int sys_posix_fsstat(int fd, struct posix_statfs *sb)
{
	struct file *f = fs_get_file_pointer((task_t *)current_task, fd);
	if(!f) return -EBADF;
	struct inode *i = f->inode;
	fs_fput((task_t *)current_task, fd, 0);
	if(!i) return -EBADF;
	return vfs_callback_fsstat(i, sb);
}

