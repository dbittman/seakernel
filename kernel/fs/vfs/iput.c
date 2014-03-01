#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <rwlock.h>
#include <atomic.h>
#include <sea/fs/inode.h>

int iput(struct inode *i)
{
	assert(i);
	rwlock_acquire(&i->rwl, RWL_WRITER);
	struct inode *parent = i->parent;
	if(parent == i) parent=0;
	if(parent) rwlock_acquire(&parent->rwl, RWL_WRITER);
	if(!i->count && i->dynamic && !(i->pipe && i->pipe->count))
		panic(0, "iput with not ref count");
	if(i->count > 0)
		sub_atomic(&i->count, 1);
	/* check if there is something preventing us from deleting the inode. */
	if(i->count || !i->dynamic || (i->pipe && i->pipe->count)) {
		if(parent) rwlock_release(&parent->rwl, RWL_WRITER);
		rwlock_release(&i->rwl, RWL_WRITER);
		return EACCES;
	}
	if(i->count || i->mount || inode_has_children(i) || i->mount_parent || i->f_count)
		panic(0, "attempting to free an inode with references! (%s:%d,%d,%d)", i->name, i->count, inode_has_children(i), i->f_count);
	do_iremove(i, 0, 1);
	if(parent) rwlock_release(&parent->rwl, RWL_WRITER);
	/* and we don't release the lock on i because it was destroyed in
	 * iremove */
	return 0;
}
