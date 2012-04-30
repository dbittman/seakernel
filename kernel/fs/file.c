/* Code to keep track of file handles */
#include <kernel.h>
#include <task.h>
#include <fs.h>

struct file *get_file_pointer(task_t *t, int n)
{
	if(!t)
		return 0;
	if(!n) 
		return t->filp;
	struct file *f = t->filp;
	while(f)
	{
		if(f->num == n)
			return f;
		f=f->next;
	}
	return 0;
}

void remove_file_pointer(task_t *t, int n)
{
	if(!t || !t->filp)
		return;
	struct file *f = get_file_pointer(t, n);
	if(!f)
		return;
	if(f->prev)
		f->prev->next = f->next;
	if(f->next)
		f->next->prev = f->prev;
	if(t->filp == f) {
		assert(!f->prev);
		t->filp = f->next;
		t->filp->prev=0;
	}
	kfree(f);
}
/* Here we find an unused filedes, and add it to the list. We rely on the list being sorted, 
 * and since this is the only function that adds to it, we can assume it is. This allows
 * for relatively efficient determining of a filedes without limit. */
int add_file_pointer_after(task_t *t, struct file *f, int after)
{
	if(!t) 
		return -1;
	struct file *p = t->filp;
	if(!p) {
		t->filp = f;
		f->num=after;
		return 0;
	}
	if(p->num > after)
	{
		p->prev = f;
		f->next = p;
		f->prev=0;
		t->filp=f;
		f->num=after;
		return 0;
	}
	int i=-1;
	while(p)
	{
		if(p->next && (p->next->num-p->num) > 1 && ((p->num+1) >= after))
		{
			i = p->num+1;
			break;
		}
		if(!p->next)
			break;
		p=p->next;
	}
	if(i != -1) {
		f->next = p->next;
		if(p->next)
			p->next->prev = f;
	} else {
		i = p->num+1;
		f->next=0;
	}
	p->next = f;
	f->prev = p;
	f->num = i;
	return i;
}

int add_file_pointer(task_t *t, struct file *f)
{
	return add_file_pointer_after(t, f, 0);
}

void copy_file_handles(task_t *p, task_t *n)
{
	if(!p || !n)
		return;
	struct file *f = p->filp;
	if(!f) {
		n->filp=0;
		return;
	}
	struct file *q=0, *base=0, *prev=0;
	q = (struct file *)kmalloc(sizeof(struct file));
	base=q;
	prev=0;
	while(f)
	{
		q->pos=0;
		q->count=1;
		change_icount(f->inode, 1);
		q->next=0;
		q->flag = f->flag;
		q->fd_flags = f->fd_flags;
		q->num = f->num;
		q->mode = f->mode;
		q->inode = f->inode;
		mutex_on(&f->inode->lock);
		f->inode->f_count++;
		mutex_off(&f->inode->lock);
		if(f->inode->pipe && !f->inode->pipe->type)
		{
			mutex_on(f->inode->pipe->lock);
			++f->inode->pipe->count;
			if(f->flag & _FWRITE) f->inode->pipe->wrcount++;
			mutex_off(f->inode->pipe->lock);
		}
		q->prev = prev;
		f=f->next;
		if(f) {
			q->next = (struct file *)kmalloc(sizeof(struct file));
			prev=q;
			q=q->next;
		} else
			q->next=0;
	}
	n->filp = base;
}
