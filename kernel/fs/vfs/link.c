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
	i = lget_idir(old, 0);
	if(!i)
		return -ENOENT;
	unlink(new);
	int ret = vfs_callback_link(i, new);
	iput(i);
	sys_utime(new, 0, 0);
	return ret;
}

int unlink(char *f)
{
	if(!f) return -EINVAL;
	struct inode *i;
	i = lget_idir(f, 0);
	if(!i)
		return -ENOENT;
	int err = 0;
	if(current_task->uid && (i->parent->mode & S_ISVTX) && (i->uid != current_task->uid))
		err = -EACCES;
	if(i->child)
		err = -EISDIR;
	if(i->f_count) {
		iput(i);
		return 0;
	}
	int ret = err ? 0 : vfs_callback_unlink(i);
	(err) ? iput(i) : iremove_force(i);
	return err ? err : ret;
}
