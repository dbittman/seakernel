#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <atomic.h>
#include <rwlock.h>
#include <sea/fs/inode.h>

int vfs_do_unlink(struct inode *i)
{
	int err = 0;
	if(current_task->thread->effective_uid && (i->parent->mode & S_ISVTX) 
		&& (i->uid != current_task->thread->effective_uid))
		err = -EACCES;
	if(S_ISDIR(i->mode))
		err = -EISDIR;
	if(!vfs_inode_get_check_permissions(i->parent, MAY_WRITE, 0))
		err = -EACCES;
	rwlock_acquire(&i->rwl, RWL_WRITER);
	if(i->f_count) {
		/* we allow any open files to keep this in existance until 
		 * it has been closed everywhere. if this flag is marked, and 
		 * we call close() and are the last process to do so, the file
		 * gets unlinked */
		i->marked_for_deletion=1;
		rwlock_release(&i->rwl, RWL_WRITER);
		iput(i);
		return 0;
	}
	if(i->count > 1 || (i->pipe && i->pipe->count))
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_unlink(i);
	if(err) {
		rwlock_release(&i->rwl, RWL_WRITER);
		iput(i);
	}
	else
		iremove_force(i);
	return err ? err : ret;
}

int vfs_link(char *old, char *new)
{
	if(!old || !new)
		return -EINVAL;
	struct inode *i;
	if((i = vfs_get_idir(new, 0)))
		vfs_do_unlink(i);
	i = vfs_get_idir(old, 0);
	if(!i)
		return -ENOENT;
	if(vfs_inode_is_directory(i) && current_task->thread->effective_uid) {
		iput(i);
		return -EPERM;
	}
	int ret = vfs_callback_link(i, new);
	iput(i);
	if(!ret) sys_utime(new, 0, 0);
	return ret;
}

int vfs_unlink(char *f)
{
	if(!f) return -EINVAL;
	struct inode *i;
	/* Hacks! */
	if(strchr(f, '*')) 
		return -ENOENT;
	i = vfs_lget_idir(f, 0);
	if(!i)
		return -ENOENT;
	return vfs_do_unlink(i);
}

int vfs_rmdir(char *f)
{
	if(!f) return -EINVAL;
	struct inode *i;
	if(strchr(f, '*'))
		return -ENOENT;
	i = vfs_lget_idir(f, 0);
	if(!i)
		return -ENOENT;
	int err = 0;
	rwlock_acquire(&i->rwl, RWL_WRITER);
	if(inode_has_children(i))
		err = -ENOTEMPTY;
	if(!vfs_inode_get_check_permissions(i->parent, MAY_WRITE, 0))
		err = -EACCES;
	if(current_task->thread->effective_uid && (i->parent->mode & S_ISVTX) 
		&& (i->uid != current_task->thread->effective_uid))
		err = -EACCES;
	if(i->f_count) {
		rwlock_release(&i->rwl, RWL_WRITER);
		iput(i);
		return 0;
	}
	if(i->count > 1)
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_rmdir(i);
	if(err) {
		rwlock_release(&i->rwl, RWL_WRITER);
		iput(i);
	}
	else
		iremove_force(i);
	return err ? err : ret;
}
