#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/fs/fcntl.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
#include <sea/dm/pipe.h>
int vfs_inode_is_directory(struct inode *i)
{
	return i ? S_ISDIR(i->mode) : 0;
}

int vfs_inode_get_ref_count(struct inode *i)
{
	return i->count;
}

int vfs_inode_get_check_permissions(struct inode *i, mode_t flag, int real_id)
{
	if(!i)
		return 0;
	uid_t uid = current_task->thread->effective_uid;
	gid_t gid = current_task->thread->effective_gid;
	if(real_id) {
		uid = current_task->thread->real_uid;
		gid = current_task->thread->real_gid;
	}
	if(uid == 0)
		return 1;
	if(uid == i->uid && (flag & i->mode))
		return 1;
	flag = flag >> 3;
	if(gid == i->gid && (flag & i->mode))
		return 1;
	flag = flag >> 3;
	return flag & i->mode;
}

static int actually_vfs_do_add_inode(struct inode *b, struct inode *i)
{
	if(!vfs_inode_is_directory(b))
		panic(0, "tried to add an inode to a file");
	i->parent = b;
	i->node=ll_insert(&b->children, i);
	return 0;
}

int vfs_do_add_inode(struct inode *b, struct inode *i, int locked)
{
	assert(b && i);
	/* we can get away with a read lock here, since the only critical
	 * counting functions that could cause problems decrease only, and
	 * they use write locks. This will save time */
	if(!locked) rwlock_acquire(&b->rwl, RWL_READER);
	/* one count per child. Tax deductions! */
	add_atomic(&b->count, 1);
	/* To save resources and time, we only create this LL if we need to.
	 * Since a large number of inodes are files, we do not need to 
	 * create this structure for each one. */
	if(!ll_is_active((&b->children))) 
		ll_create(&b->children);
	actually_vfs_do_add_inode(b, i);
	if(!locked) rwlock_release(&b->rwl, RWL_READER);
	return 0;
}

int vfs_free_inode(struct inode *i, int recur)
{
	assert(i && !i->parent);
	assert(recur || !i->children.head);
	vfs_destroy_flocks(i);
	if(i->pipe)
		dm_free_pipe(i);
	if(i->start)
		kfree((void *)i->start);
	rwlock_release(&i->rwl, RWL_WRITER);
	rwlock_destroy(&i->rwl);
	if(recur)
	{
		struct inode *c;
		struct llistnode *cur, *nxt;
		ll_for_each_entry_safe((&i->children), cur, nxt, struct inode *, c)
		{
			ll_remove(&i->children, cur);
			c->node=0;
			c->parent=0;
			vfs_free_inode(c, 1);
		}
	}
	ll_destroy(&i->children);
	kfree(i);
	return 0;
}

int vfs_do_iremove(struct inode *i, int flag, int locked)
{
	assert(i);
	struct inode *parent = i->parent;
	assert(parent != i);
	if(!locked && parent) rwlock_acquire(&parent->rwl, RWL_WRITER);
	if(!flag && (i->count || i->children.head) && flag != 3)
		panic(0, "Attempted to iremove inode with count > 0 or children! %d (%s)", 
			i->count, i->name);
	/* remove the count added by having this child */
	i->parent=0;
	if(parent) {
		ll_remove(&parent->children, i->node);
		if(!locked) {
			rwlock_release(&parent->rwl, RWL_WRITER);
			vfs_iput(parent);
		} else
			sub_atomic(&parent->count, 1);
	}
	if(flag != 3)
		vfs_free_inode(i, (flag == 2) ? 1 : 0);
	return 0;
}
