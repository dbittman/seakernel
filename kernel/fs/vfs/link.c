#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
#include <sea/dm/pipe.h>
#include <sea/libgen.h>
#include <sea/fs/dir.h>
#include <sea/errno.h>
#include <sea/string.h>

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
	i->mtime = time_get_epoch();	
	sync_inode_tofs(i);
	rwlock_acquire(&i->rwl, RWL_WRITER);
	if(i->f_count) {
		/* we allow any open files to keep this in existance until 
		 * it has been closed everywhere. if this flag is marked, and 
		 * we call close() and are the last process to do so, the file
		 * gets unlinked */
		i->marked_for_deletion=1;
		rwlock_release(&i->rwl, RWL_WRITER);
		vfs_iput(i);
		return 0;
	}
	if(i->count > 1 || (i->pipe && i->pipe->count))
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_unlink(i);
	if(err) {
		rwlock_release(&i->rwl, RWL_WRITER);
		vfs_iput(i);
	}
	else
		iremove_force(i);
	return err ? err : ret;
}

int vfs_link(char *old, char *new)
{
	if(!old || !new)
		return -EINVAL;
	struct inode *i, *o, *parent;

	/* check parent dir for new, and permissions */
	char newdir[strlen(new)+1];
	strncpy(newdir, new, strlen(new)+1);
	parent = vfs_get_idir(dirname(newdir), 0);
	if(!parent)
		return -ENOENT;
	if(!vfs_inode_get_check_permissions(parent, MAY_WRITE, 0))
	{
		vfs_iput(parent);
		return -EACCES;
	}
	i = vfs_get_idir(old, 0);
	if(!i) {
		vfs_iput(parent);
		return -ENOENT;
	}
	/* check cross-filesystem link */
	if(i->fs_idx != parent->fs_idx || i->sb_idx != parent->sb_idx)
	{
		vfs_iput(parent);
		vfs_iput(i);
		return -EXDEV;
	}
	vfs_iput(parent);

	if((o = vfs_get_idir(new, 0)))
		vfs_do_unlink(o);

	if(vfs_inode_is_directory(i) && current_task->thread->effective_uid) {
		vfs_iput(i);
		return -EPERM;
	}
	int ret = vfs_callback_link(i, new);
	i->mtime = time_get_epoch();
	sync_inode_tofs(i);
	vfs_iput(i);
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
	i->mtime = time_get_epoch();
	sync_inode_tofs(i);
	rwlock_acquire(&i->rwl, RWL_READER);
	if(!vfs_directory_is_empty(i))
		err = -ENOTEMPTY;
	rwlock_escalate(&i->rwl, RWL_WRITER);
	if(!vfs_inode_get_check_permissions(i->parent, MAY_WRITE, 0))
		err = -EACCES;
	if(current_task->thread->effective_uid && (i->parent->mode & S_ISVTX) 
		&& (i->uid != current_task->thread->effective_uid))
		err = -EACCES;
	if(i->f_count) {
		rwlock_release(&i->rwl, RWL_WRITER);
		vfs_iput(i);
		return 0;
	}
	if(i->count > 1)
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_rmdir(i);
	if(err) {
		rwlock_release(&i->rwl, RWL_WRITER);
		vfs_iput(i);
	}
	else
		iremove_force(i);
	return err ? err : ret;
}
