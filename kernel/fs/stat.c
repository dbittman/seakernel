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
#include <sea/fs/pipe.h>
#include <sea/errno.h>
int sys_isatty(int f)
{
	struct file *file = file_get(f);
	if(!file) return -EBADF;
	struct inode *inode = file->inode;
	/* TODO */
	if(inode->devdata) {
		file_put(file);
		return 1;
	}
	file_put(file);
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
	/* HACK: some filesystems might not set this... */
	if(!tmp->st_blksize)
		tmp->st_blksize = 512;
}

int sys_stat(char *f, struct stat *statbuf, bool lin)
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

int fs_inode_dirempty(struct inode *dir)
{
	char tmp[512];
	unsigned int n;
	int r = fs_callback_inode_getdents(dir, 0, (void *)tmp, 512, &n);
	struct dirent_posix *des = (void *)tmp;
	while((addr_t)des < (addr_t)(tmp + r)) {
		if(strcmp(des->d_name, ".") && strcmp(des->d_name, "..")) {
			return 0;
		}
		des = (void *)((addr_t)des + des->d_reclen);
	}
	return 1;
}

int sys_getdents(int fd, struct dirent_posix *dirs, unsigned int count)
{
	struct file *f = file_get(fd);
	if(!f) return -EBADF;

	unsigned nex;
	if(!vfs_inode_check_permissions(f->inode, MAY_READ, 0)) {
		file_put(f);
		return -EACCES;
	}
	rwlock_acquire(&f->inode->lock, RWL_READER);
	int r = fs_callback_inode_getdents(f->inode, f->pos, dirs, count, &nex);
	rwlock_release(&f->inode->lock, RWL_READER);
	f->pos = nex;

	file_put(f);
	return r;
}

int sys_fstat(int fp, struct stat *sb)
{
	if(!sb)
		return -EINVAL;
	struct file *f = file_get(fp);
	if(!f) return -EBADF;
	do_stat(f->inode, sb);
	file_put(f);
	return 0;
}

int sys_posix_fsstat(int fd, struct posix_statfs *sb)
{
	struct file *f = file_get(fd);
	if(!f) return -EBADF;
	struct inode *i = f->inode;
	int r = fs_callback_fs_stat(i->filesystem, sb);
	file_put(f);
	return r;
}

