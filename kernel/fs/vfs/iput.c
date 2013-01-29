#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <rwlock.h>
#include <atomic.h>

int iput(struct inode *i)
{
	assert(i);
	rwlock_acquire(&i->rwl, RWL_WRITER);
	struct inode *parent = i->parent;
	if(i->count > 0)
		sub_atomic(&i->count, 1);
	/* check if there is something preventing us from deleting the inode. */
	#warning "figure out better pipe ref counting"
	if(i->count || !i->dynamic || (i->pipe && i->pipe->count)) {
		rwlock_release(&i->rwl, RWL_WRITER);
		return EACCES;
	}
	//if(!parent) panic(0, "warning - this logic is untested: deleting inode with null parent");
	if(i->count || i->mount || inode_has_children(i) || i->mount_parent || i->f_count)
		panic(0, "attempting to free an inode with references!");
	#warning "check the structure for other possible references"
	iremove(i);
	/* and we don't release the lock on i because it was destroyed in
	 * iremove */
	return 0;
}
