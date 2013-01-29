#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <fcntl.h>
#include <atomic.h>
#include <rwlock.h>

int is_directory(struct inode *i)
{
	return i ? S_ISDIR(i->mode) : 0;
}

int get_ref_count(struct inode *i)
{
	return i->count;
}

int permissions(struct inode *i, mode_t flag)
{
	if(!i)
		return 0;
	if(ISGOD(current_task))
		return 1;
	if(current_task->uid == i->uid && (flag & i->mode))
		return 1;
	flag = flag >> 3;
	if(current_task->gid == i->gid && (flag & i->mode))
		return 1;
	flag = flag >> 3;
	return flag & i->mode;
}

int actually_do_add_inode(struct inode *b, struct inode *i)
{
	if(!is_directory(b))
		panic(0, "tried to add an inode to a file");
	i->parent = b;
	i->node=ll_insert(&b->children, i);
	return 0;
}

int do_add_inode(struct inode *b, struct inode *i, int locked)
{
	assert(b && i);
	/* we can get away with a read lock here, since the only critical
	 * counting functions that could cause problems decrease only, and
	 * they use write locks. This will save time */
	if(!locked) rwlock_acquire(&b->rwl, RWL_READER);
	else assert((b->rwl.locks) >= 2);
	/* one count per child. Tax deductions! */
	add_atomic(&b->count, 1);
	/* To save resources and time, we only create this LL if we need to.
	 * Since a large number of inodes are files, we do not need to 
	 * create this structure for each one. */
	if(!ll_is_active((&b->children))) {
		rwlock_escalate(&b->rwl, RWL_WRITER);
		ll_create(&b->children);
		if(!locked) rwlock_release(&b->rwl, RWL_WRITER);
		else rwlock_escalate(&b->rwl, RWL_READER);
	} else if(!locked)
		rwlock_release(&b->rwl, RWL_READER);
	return actually_do_add_inode(b, i);
}

int recur_total_refs(struct inode *i)
{
	/* TODO: WARNING: this is dangerous. We could VERY easily end up
	 * deadlocking the system by pulling a stunt like this. Maybe we
	 * should have inc_parent_times for inodes too? */
	rwlock_acquire(&i->rwl, RWL_READER);
	int x = i->count;
	struct inode *c;
	struct llistnode *cur;
	ll_for_each_entry((&i->children), cur, struct inode *, c)
	{
		x += recur_total_refs(c);
	}
	rwlock_release(&i->rwl, RWL_READER);
	return x;
}

int free_inode(struct inode *i, int recur)
{
	assert(i && !i->parent);
	assert(recur || !i->children.head);
	destroy_flocks(i);
	if(i->pipe)
		free_pipe(i);
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
			ll_maybe_reset_loop((&i->children), cur, nxt);
			c->node=0;
			free_inode(c, 1);
		}
	}
	ll_destroy(&i->children);
	kfree(i);
	return 0;
}

int do_iremove(struct inode *i, int flag)
{
	assert(i);
	struct inode *parent = i->parent;
	assert(parent);
	assert(parent != i);
	rwlock_acquire(&parent->rwl, RWL_WRITER);
	if(!flag && (i->count || i->children.head) && flag != 3)
		panic(0, "Attempted to iremove inode with count > 0 or children! %d (%s)", 
			i->count, i->name);
	/* remove the count added by having this child */
	i->parent=0;
	#warning "we should iput here..."
	sub_atomic(&parent->count, 1);
	ll_remove(&parent->children, i->node);
	rwlock_release(&parent->rwl, RWL_WRITER);
	if(flag != 3)
		free_inode(i, (flag == 2) ? 1 : 0);
	return 0;
}
