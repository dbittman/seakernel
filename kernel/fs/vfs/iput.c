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
	rwlock_acquire(&i->rwl, RWL_READER);
	struct inode *parent = i->parent;
	if(parent && parent != i)
		rwlock_acquire(&parent->rwl, RWL_WRITER);
	/* decrease count by one. we don't need a write lock here since we
	 * know that we own this inode (and have a read lock on it), so an
	 * atomic operation will suffice */
	if(i->count > 0)
		sub_atomic(&i->count, 1);
	/* check if there is something preventing us from deleting the inode. */
	if(i->count || inode_has_children(i) || i->mount || i->mount_parent || !i->dynamic 
			|| i->f_count || i->required || !parent || parent->required
			|| (i->pipe && i->pipe->count)) {
		if(parent && parent != i) 
			rwlock_release(&parent->rwl, RWL_WRITER);
		rwlock_release(&i->rwl, RWL_READER);
		return EACCES;
	}
	/* if we've gotten here, we will need to acquire a write lock so that
	 * we may safely delete the inode */
	rwlock_escalate(&i->rwl, RWL_WRITER);
	iremove(i);
	if(parent && parent != i) rwlock_release(&parent->rwl, RWL_WRITER);
	/* and we don't release the lock on i because it was destroyed in
	 * iremove */
	return 0;
}

/* After calling this, you MAY NOT access i any more as the data 
 * may have been freed. */
int iput(struct inode *i)
{
	assert(i);
	return do_iput(i);
}
