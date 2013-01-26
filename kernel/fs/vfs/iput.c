#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <rwlock.h>
int do_iput(struct inode *i)
{
	if(!i)
		return EINVAL;
	struct inode *parent = i->parent;
	if(parent && parent != i) 
		rwlock_acquire(&parent->rwl, RWL_WRITER);	
	if(i->count > 0)
		i->count--;
	if(i->count || inode_has_children(i) || i->mount || i->mount_parent || !i->dynamic 
			|| i->f_count || i->required || !parent || parent->required
			|| (i->pipe && i->pipe->count)) {
		if(parent && parent != i) 
			rwlock_release(&parent->rwl, RWL_WRITER);
		return EACCES;
	}
	i->unreal=1;
	iremove(i);
	if(parent && parent != i) rwlock_release(&parent->rwl, RWL_WRITER);
	return 0;
}

/* After calling this, you MAY NOT access i any more as the data 
 * may have been freed. To call iput, you MUST have a RWL_WRITER lock
 * on the inode! Also, iput will release that lock for you! */
int iput(struct inode *i)
{
	if(!i)
		return -EINVAL;
	assert(!i->unreal);
	int ret = do_iput(i);
	if(ret)
		rwlock_release(&i->rwl, RWL_WRITER);
	return -ret;
}
