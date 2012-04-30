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
	i = get_idir(old, 0);
	if(!i)
		return -ENOENT;
	unlink(new);
	int ret = -EINVAL;
	if(i->i_ops)
		if(i->i_ops->link)
			ret = i->i_ops->link(i, new);
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
	if(!permissions(i->parent, MAY_WRITE)){
		iput(i);
		return -EACCES;
	}
	if(i->child) {
		iput(i);
		return -EISDIR;
	}
	if(i->f_count) { /* HACK: This should queue the file for deletion */
		iput(i);
		return 0;
	}
	int ret=-EINVAL;
	if(i->i_ops)
		if(i->i_ops->unlink)
			ret = (i->i_ops->unlink(i));
	iput(i);
	return ret;
}
