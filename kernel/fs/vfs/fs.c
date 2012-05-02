#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
extern struct inode *devfs_root, *procfs_root;

int rename(char *f, char *nname)
{
	if(!f || !nname) return -EINVAL;
	int ret = link(f, nname);
	if(ret >= 0)
		ret = unlink(f);
	return ret;
}

int sync_inode_tofs(struct inode *i)
{
	if(!i)
		return -EINVAL;
	int ret = -EINVAL;
	if(i && i->i_ops && i->i_ops->sync_inode)
		ret = i->i_ops->sync_inode(i);
	return ret;
}

struct inode *sys_create(char *path)
{
	return cget_idir(path, 0, current_task->cmask);
}
