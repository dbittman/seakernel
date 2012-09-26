#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <fcntl.h>
int is_directory(struct inode *i)
{
	return i ? S_ISDIR(i->mode) : 0;
}

int change_icount(struct inode *i, int c)
{
	int ret=0;
	if(!i) return 0;
	mutex_on(&i->lock);
	ret = (i->count += c);
	mutex_off(&i->lock);
	return ret;
}

int change_ireq(struct inode *i, int c)
{
	int ret=0;
	if(!i) return 0;
	mutex_on(&i->lock);
	ret = (i->required += c);
	mutex_off(&i->lock);
	return ret;
}

int get_ref_count(struct inode *i)
{
	return i ? i->count : 0;
}

int get_f_ref_count(struct inode *i)
{
	return i ? i->f_count : 0;
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

int do_add_inode(struct inode *b, struct inode *i)
{
	if(!is_directory(b))
		panic(0, "tried to add an inode to a file");
	i->parent = b;
	i->node=ll_insert(&b->children, i);
	return 0;
}

int add_inode(struct inode *b, struct inode *i)
{
	assert(b && i);
	int ret;
	/* To save resources and time, we only create this LL if we need to.
	 * Since a large number of inodes are files, we do not need to 
	 * create this structure for each one. */
	if(!ll_is_active((&b->children)))
		ll_create(&b->children);
	mutex_on(&b->lock);
	ret = do_add_inode(b, i);
	mutex_off(&b->lock);
	return ret;
}

int recur_total_refs(struct inode *i)
{
	int x = i->count;
	struct inode *c;
	struct llistnode *cur;
	ll_for_each_entry((&i->children), cur, struct inode *, c)
	{
		x += recur_total_refs(c);
	}
	return x;
}

int free_inode(struct inode *i, int recur)
{
	assert(i);
	assert(recur || !i->children.head);
	destroy_flocks(i);
	if(i->pipe)
		free_pipe(i);
	if(i->start)
		kfree((void *)i->start);
	destroy_mutex(&i->lock);
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
	if(!i) return -1;
	struct inode *parent = i->parent;
	if(!parent) return -1;
	assert(parent != i);
	mutex_on(&parent->lock);
	if(!flag && (get_ref_count(i) || i->children.head) && flag != 3)
		panic(0, "Attempted to iremove inode with count > 0 or children! (%s)", 
			i->name);
	if(flag != 3) 
		i->unreal=1;
	ll_remove(&parent->children, i->node);
	mutex_off(&parent->lock);
	if(flag != 3)
		free_inode(i, (flag == 2) ? 1 : 0);
	return 0;
}
