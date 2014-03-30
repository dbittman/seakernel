/* Provides callbacks for VFS functions to drivers */

#include <sea/kernel.h>
#include <sea/fs/inode.h>


#define CALLBACK_NOFUNC_ERROR -ENOTSUP

int vfs_callback_read (struct inode *i, off_t a, size_t b, char *d)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->read)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->read(i, a, b, d);
}

int vfs_callback_write (struct inode *i, off_t a, size_t b, char *d)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->write)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->write(i, a, b, d);
}

int vfs_callback_select (struct inode *i, unsigned int m)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->select)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->select(i, m);
}

struct inode *vfs_callback_create (struct inode *i,char *d, mode_t m)
{
	if(!i) return 0;
	if(!i->i_ops || !i->i_ops->create)
		return 0;
	return i->i_ops->create(i, d, m);
}

struct inode *vfs_callback_lookup (struct inode *i,char *d)
{
	if(!i) return 0;
	if(!i->i_ops || !i->i_ops->lookup)
		return 0;
	return i->i_ops->lookup(i, d);
}

struct inode *vfs_callback_readdir (struct inode *i, unsigned n)
{
	if(!i) return 0;
	if(!i->i_ops || !i->i_ops->readdir)
		return 0;
	return i->i_ops->readdir(i, n);
}

int vfs_callback_link (struct inode *i, char *d)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->link)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->link(i, d);
}

int vfs_callback_unlink (struct inode *i)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->unlink)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->unlink(i);
}

int vfs_callback_rmdir (struct inode *i)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->rmdir)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->rmdir(i);
}

int vfs_callback_sync_inode (struct inode *i)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->sync_inode)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->sync_inode(i);
}

int vfs_callback_unmount (struct inode *i, unsigned int n)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->unmount)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->unmount(i, n);
}

int vfs_callback_fsstat (struct inode *i, struct posix_statfs *s)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->fsstat)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->fsstat(i, s);
}

int vfs_callback_fssync (struct inode *i)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->fssync)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->fssync(i);
}

int vfs_callback_update (struct inode *i)
{
	if(!i) return -EINVAL;
	if(!i->i_ops || !i->i_ops->update)
		return CALLBACK_NOFUNC_ERROR;
	return i->i_ops->update(i);
}
