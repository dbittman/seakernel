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

int permissions(struct inode *i, int flag)
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
	struct inode *tmp = b->child;
	b->child = i;
	i->next = tmp;
	i->parent = b;
	i->prev=0;
	if(tmp)
		tmp->prev=i;
	return 0;
}

int add_inode(struct inode *b, struct inode *i)
{
	assert(b && i);
	int ret;
	mutex_on(&b->lock);
	ret = do_add_inode(b, i);
	mutex_off(&b->lock);
	return ret;
}

int recur_total_refs(struct inode *i)
{
	int x = i->count;
	struct inode *c = i->child;
	while(c) {
		x += recur_total_refs(c);
		c=c->next;
	}
	return x;
}

int free_inode(struct inode *i, int recur)
{
	assert(i);
	assert(recur || !i->child);
	destroy_flocks(i);
	if(i->pipe)
		free_pipe(i);
	if(i->start)
		kfree((void *)i->start);
	destroy_mutex(&i->lock);
	if(recur)
	{
		while(i->child)
		{
			struct inode *c = i->child;
			i->child=i->child->next;
			free_inode(c, 1);
		}
	}
	kfree(i);
	return 0;
}

int do_iremove(struct inode *i, int flag)
{
	if(!i) return -1;
	struct inode *parent = i->parent;
	if(parent && parent != i) mutex_on(&parent->lock);
	if(!flag && (get_ref_count(i) || i->child) && flag != 3)
		panic(0, "Attempted to iremove inode with count > 0 or children! (%s)", 
			i->name);
	if(flag != 3) 
		i->unreal=1;
	struct inode *prev = i->prev;
	if(prev)
		prev->next = i->next;
	if(parent && parent->child == i && parent != i)
		parent->child=i->next;
	if(i->next)
		i->next->prev = prev;
	if(parent && parent != i) mutex_off(&parent->lock);
	if(flag != 3)
		free_inode(i, (flag == 2) ? 1 : 0);
	return 0;
}
