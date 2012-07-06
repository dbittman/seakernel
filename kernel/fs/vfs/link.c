#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int link(char *old, char *new)
{
	if(!old || !new)
		return -EINVAL;
	struct inode *i;
	if((i = get_idir(new, 0)))
		do_unlink(i);
	i = get_idir(old, 0);
	if(!i)
		return -ENOENT;
	mutex_on(&i->lock);
	int ret = vfs_callback_link(i, new);
	iput(i);
	if(!ret) sys_utime(new, 0, 0);
	return ret;
}

int do_unlink(struct inode *i)
{
	int err = 0;
	if(current_task->uid && (i->parent->mode & S_ISVTX) 
			&& (i->uid != current_task->uid))
		err = -EACCES;
	if(i->child)
		err = -EISDIR;
	if(!permissions(i->parent, MAY_WRITE))
		err = -EACCES;
	if(i->f_count) {
		/* we allow any open files to keep this in existance until 
		 * it has been closed everywhere. if this flag is marked, and 
		 * we call close() and are the last process to do so, the file
		 * gets unlinked */
		i->marked_for_deletion=1;
		iput(i);
		return 0;
	}
	if(i->count > 1 || i->required || i->mount || i->mount_parent)
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_unlink(i);
	(err) ? iput(i) : iremove_force(i);
	return err ? err : ret;
}

int unlink(char *f)
{
	if(!f) return -EINVAL;
	struct inode *i;
	if(strchr(f, '*'))
		return -ENOENT;
	i = lget_idir(f, 0);
	if(!i)
		return -ENOENT;
	return do_unlink(i);
}

int rmdir(char *f)
{
	if(!f) return -EINVAL;
	struct inode *i;
	if(strchr(f, '*'))
		return -ENOENT;
	i = lget_idir(f, 0);
	if(!i)
		return -ENOENT;
	int err = 0;
	if(i->child)
		err = -ENOTEMPTY;
	if(!permissions(i->parent, MAY_WRITE))
		err = -EACCES;
	if(i->f_count) {
		iput(i);
		return 0;
	}
	if(i->count > 1 || i->required || i->mount || i->mount_parent)
		err = -EBUSY;
	int ret = err ? 0 : vfs_callback_rmdir(i);
	(err) ? iput(i) : iremove_force(i);
	return err ? err : ret;
}
