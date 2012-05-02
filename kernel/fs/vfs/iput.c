#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int do_iput(struct inode *i)
{
	if(!i)
		return EINVAL;
	if(i->count > 0)
		change_icount(i, -1);
	if(i->count > 0 || i->child || i->mount_ptr || i->r_mount_ptr || !i->dynamic || i->f_count || i->required)
		return EACCES;
	struct inode *parent = i->parent;
	mutex_off(&i->lock);
	if(parent) mutex_on(&parent->lock);
	if(i->count > 0)
	{
		/* Once we have gained exclusive access to parent, we must check that that
		 * this inode is truly free. It may have been allocated before we grabbed
		 * the mutex.*/
		if(parent) mutex_off(&parent->lock);
		return EACCES;
	}
	i->unreal=1;
	int g = iremove(i);
	if(parent) mutex_off(&parent->lock);
	return g;
}

/* After calling this, you MAY NOT access i any more as the data may have been freed */
int iput(struct inode *i)
{
	if(!i)
		return -EINVAL;
	if(i->unreal)
		return 0;
	mutex_on(&i->lock);
	int ret = do_iput(i);
	if(ret)
		reset_mutex(&i->lock);
	return -ret;
}
