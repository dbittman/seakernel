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
	struct inode *parent = i->parent;
	if(parent && parent != i) mutex_on(&parent->lock);	
	if(i->count > 0)
		i->count--;
	if(i->count || inode_has_children(i) || i->mount || i->mount_parent || !i->dynamic 
			|| i->f_count || i->required || !parent || parent->required
			|| (i->pipe && i->pipe->count)) {
		if(parent && parent != i) mutex_off(&parent->lock);
		return EACCES;
	}
	i->unreal=1;
	iremove(i);
	if(parent && parent != i) mutex_off(&parent->lock);
	return 0;
}

/* After calling this, you MAY NOT access i any more as the data 
 * may have been freed */
int iput(struct inode *i)
{
	if(!i)
		return -EINVAL;
	assert(!i->unreal);
	mutex_on(&i->lock);
	int ret = do_iput(i);
	if(ret)
		reset_mutex(&i->lock);
	return -ret;
}
