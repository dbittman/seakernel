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

static struct inode *__fs_do_open_resolve__(const char *path, int *err, struct dirent **dir)
{
	struct dirent *de = fs_path_resolve(path, 0, err);
	if(!de)
		return 0;
	struct inode *node = fs_dirent_readinode(de, true);
	if(!node) {
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

struct file *fs_file_open(const char *name, int flags, mode_t mode, int *error)
{
	struct inode *inode;
	struct dirent *dirent = 0;
	mode = (mode & ~0xFFF) | ((mode & 0xFFF) & (~(current_process->cmask & 0xFFF)));

	if(flags & _FCREAT)
		inode =	fs_path_resolve_create_get(name, 0, S_IFREG | mode, error, &dirent);
	else
		inode = __fs_do_open_resolve__(name, error, &dirent);

	if(!inode) {
		if(dirent)
			vfs_dirent_release(dirent);
		return NULL;
	}

	if(flags & _FCREAT && flags & _FEXCL && !*error) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		if(error)
			*error = -EEXIST;
		return 0;
	}

	if((flags & _FREAD) && !vfs_inode_check_permissions(inode, MAY_READ, 0)) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		if(error)
			*error = -EACCES;
		return NULL;
	}
	if((flags & _FWRITE) && !vfs_inode_check_permissions(inode, MAY_WRITE, 0)) {
		vfs_icache_put(inode);
		vfs_dirent_release(dirent);
		if(error)
			*error = -EACCES;
		return NULL;
	}
	if(flags & _FTRUNC && S_ISREG(inode->mode))
	{
		inode->length=0;
		inode->ctime = inode->mtime = time_get_epoch();
		vfs_inode_set_dirty(inode);
	}

	return file_create(inode, dirent, flags);
}

int sys_open(char *name, int flags, mode_t mode)
{
	int error = 0;
	struct file *f = fs_file_open(name, flags+1, mode, &error);
	if(!f) {
		return error;
	}
	int fdnum = file_add_filedes(f, 0);
	file_put(f);
	return fdnum;
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
