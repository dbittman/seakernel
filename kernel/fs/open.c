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
#include <stdatomic.h>
#include <sea/fs/file.h>
#include <sea/fs/pipe.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/fs/dir.h>

static struct inode *__fs_do_open_resolve__(char *path, int *err, struct dirent **dir)
{
	struct dirent *de = fs_path_resolve(path, 0, err);
	if(!de)
		return 0;
	struct inode *node = fs_dirent_readinode(de, true);
	if(!node) {
		vfs_dirent_release(de);
		*err = -EIO;
		return 0;
	}
	if((*err = fs_resolve_iter_symlink(&de, &node, 0)))
		return 0;
	*dir = de;
	node = fs_resolve_mount(node);
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
	mode_t mode = (_mode & ~0xFFF) | ((_mode&0xFFF) & (~(current_process->cmask&0xFFF)));
	
	inode = (flags & _FCREAT) ?
				fs_path_resolve_create_get(name, 0, 
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
	int ret;
	f = file_create(inode, dirent, flags);
	f->pos=0;
	f->fd_flags &= ~FD_CLOEXEC; //TODO ???
	int fdnum = file_add_filedes(f, 0);
	if(fdnum == -1) {
		file_put(f);
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		*error = -EMFILE;
		return 0;
	}
	if(num) *num = fdnum;
	//if(S_ISCHR(inode->mode) && !(flags & _FNOCTTY))
	//	dm_char_rw(OPEN, inode->phys_dev, 0, 0);
	if(flags & _FTRUNC && S_ISREG(inode->mode))
	{
		inode->length=0;
		inode->ctime = inode->mtime = time_get_epoch();
		vfs_inode_set_dirty(inode);
	}
	return f;
}

int sys_open_posix(char *name, int flags, mode_t mode)
{
	int error=0, num;
	struct file *f = fs_do_sys_open(name, flags, mode, &error, &num);
	if(!f) {
		return error;
	}
	file_put(f);
	return num;
}

int sys_open(char *name, int flags)
{
	return sys_open_posix(name, flags, 0);
}

static int duplicate(struct process *t, int fp, int n)
{
	struct file *f = file_get(fp);
	if(!f)
		return -EBADF;
	int r = file_add_filedes(f, n);
	file_put(f);
	return r;
}

int sys_dup(int f)
{
	return duplicate(current_process, f, 0);
}

int sys_dup2(int f, int n)
{
	return duplicate(current_process, f, n);
}
