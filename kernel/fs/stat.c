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
	if(S_ISCHR(inode->mode))
	{
		if(MAJOR(inode->dev) == 3 || MAJOR(inode->dev) == 4) {
			return 1;
		}
	}
	return 0;
}

int sys_getpath(int f, char *b)
{
	if(!b) return -EINVAL;
	struct file *file = get_file_pointer((task_t *) current_task, f);
	if(!file)
		return -EBADF;
	struct inode *inode = file->inode;
	return get_path_string(inode, b);
}

int do_stat(struct inode * inode, struct stat * statbuf)
{
	unsigned int i;
	struct stat tmp;
	tmp.st_dev = inode->dev;
	tmp.st_ino = inode->num;
	tmp.st_mode = inode->mode;
	tmp.st_uid = inode->uid;
	tmp.st_gid = inode->gid;
	tmp.st_rdev = inode->dev;
	tmp.st_size = inode->len;
	tmp.st_blocks = inode->nblocks;
	tmp.st_nlink = inode->nlink;
	tmp.st_atime = inode->atime;
	tmp.st_mtime = inode->mtime;
	tmp.st_ctime = inode->ctime;
	memcpy(statbuf, &tmp, sizeof(struct stat));
	return (0);
}

int sys_stat(char *f, struct stat *statbuf, int lin)
{
	if(!f || !statbuf) return -EINVAL;
	struct inode *i;
	i = (struct inode *) (lin ? lget_idir(f, 0) : get_idir(f, 0));
	if(!i)
		return -ENOENT;
	int r = do_stat(i, statbuf);
	iput(i);
	return r;
}

int sys_dirstat(char *dir, unsigned num, char *namebuf, struct stat *statbuf)
{
	if(!namebuf || !statbuf || !dir)
		return -EINVAL;
	struct inode *i = read_dir(dir, num);
	if(!i)
		return -ESRCH;
	do_stat(i, statbuf);
	strcpy(namebuf, i->name);
	iput(i);
	return 0;
}

int sys_fstat(int fp, struct stat *sb)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	if(!sb)
		return -EINVAL;
	return do_stat(f->inode, sb);
}

int sys_fsstat(int fp, struct fsstat *fss)
{
	if(!fss) return -EINVAL;
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f)
		return -EBADF;
	return do_fs_stat(f->inode, fss);
}
