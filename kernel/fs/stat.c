/* kernel/fs/stat.c: Copyright (c) 2010 Daniel Bittman
 * Provides functions for gaining information about a file */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <sys/fcntl.h>

int sys_isatty(int f)
{
	struct file *file = get_file_pointer((task_t *) current_task, f);
	if(!file) return -EBADF;
	struct inode *inode = file->inode;
	if(S_ISCHR(inode->mode) && (MAJOR(inode->dev) == 3 || MAJOR(inode->dev) == 4))
		return 1;
	return 0;
}

int sys_getpath(int f, char *b, int len)
{
	if(!b) return -EINVAL;
	struct file *file = get_file_pointer((task_t *) current_task, f);
	if(!file)
		return -EBADF;
	return get_path_string(file->inode, b, len);
}

void do_stat(struct inode * inode, struct stat * tmp)
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
}

int sys_stat(char *f, struct stat *statbuf, int lin)
{
	if(!f || !statbuf) return -EINVAL;
	struct inode *i;
	i = (struct inode *) (lin ? lget_idir(f, 0) : get_idir(f, 0));
	if(!i)
		return -ENOENT;
	//kprintf("%d: STAT: %s: %x: %d\n", current_task->pid, f, i, lin);
	do_stat(i, statbuf);
	iput(i);
	return 0;
}

int sys_dirstat(char *dir, unsigned num, char *namebuf, struct stat *statbuf)
{
	if(!namebuf || !statbuf || !dir)
		return -EINVAL;
	struct inode *i = read_dir(dir, num);
	if(!i)
		return -ESRCH;
	do_stat(i, statbuf);
	strncpy(namebuf, i->name, 128);
	if(i->dynamic) 
	{
		rwlock_acquire(&i->rwl, RWL_WRITER);
		free_inode(i, 0);
	}
	return 0;
}

int sys_dirstat_fd(int fd, unsigned num, char *namebuf, struct stat *statbuf)
{
	if(!namebuf || !statbuf)
		return -EINVAL;
	struct file *f = get_file_pointer((task_t *)current_task, fd);
	if(!f) return -EBADF;
	struct inode *i = read_idir(f->inode, num);
	if(!i)
		return -ESRCH;
	do_stat(i, statbuf);
	strncpy(namebuf, i->name, 128);
	if(i->dynamic) 
	{
		rwlock_acquire(&i->rwl, RWL_WRITER);
		free_inode(i, 0);
	}
	return 0;
}

int sys_fstat(int fp, struct stat *sb)
{
	if(!sb)
		return -EINVAL;
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	do_stat(f->inode, sb);
	return 0;
}

int sys_posix_fsstat(int fd, struct posix_statfs *sb)
{
	struct file *f = get_file_pointer((task_t *)current_task, fd);
	if(!f) return -EBADF;
	struct inode *i = f->inode;
	if(!i) return -EBADF;
	return vfs_callback_fsstat(i, sb);
}
