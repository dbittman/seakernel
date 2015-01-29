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
#include <sea/errno.h>
int sys_isatty(int f)
{
	struct file *file = fs_get_file_pointer((task_t *) current_task, f);
	if(!file) return -EBADF;
	struct inode *inode = file->inode;
	if(S_ISCHR(inode->mode) && (MAJOR(inode->phys_dev) == 3 || MAJOR(inode->phys_dev) == 4)) {
		fs_fput((task_t *)current_task, f, 0);
		return 1;
	}
	fs_fput((task_t *)current_task, f, 0);
	return 0;
}

static void do_stat(struct inode * inode, struct stat * tmp)
{
	assert(inode && tmp);
	tmp->st_dev = inode->phys_dev;
	tmp->st_ino = inode->id;
	tmp->st_mode = inode->mode;
	tmp->st_uid = inode->uid;
	tmp->st_gid = inode->gid;
	tmp->st_rdev = inode->phys_dev;
	tmp->st_size = inode->length;
	tmp->st_blocks = inode->nblocks;
	tmp->st_nlink = inode->nlink;
	tmp->st_atime = inode->atime;
	tmp->st_mtime = inode->mtime;
	tmp->st_ctime = inode->ctime;
	tmp->st_blksize = inode->blocksize;
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
	int res;
	i = (struct inode *) (lin ? fs_path_resolve_inode(f, RESOLVE_NOLINK, &res) : fs_path_resolve_inode(f, 0, &res));

	if(!i)
		return res;
	do_stat(i, statbuf);
	vfs_icache_put(i);
	return 0;
}

int sys_getdents(int fd, struct dirent_posix *dirs, unsigned int count)
{
	struct file *f = fs_get_file_pointer((task_t *)current_task, fd);
	if(!f) return -EBADF;

	unsigned nex;
	int r = fs_callback_inode_getdents(f->inode, f->pos, dirs, count, &nex);
	f->pos = nex;

	fs_fput((task_t *)current_task, fd, 0);
	return r;
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
	int r = fs_callback_fs_stat(i->filesystem, sb);
	fs_fput((task_t *)current_task, fd, 0);
	return r;
}

