/* Code to keep track of file handles */
#include <kernel.h>
#include <task.h>
#include <fs.h>

struct file_ptr *get_file_handle(task_t *t, int n)
{
	if(!t)
		return 0;
	struct file_ptr *f = t->filp;
	while(f)
	{
		if(f->num == n)
			return f;
		f=f->next;
	}
	return 0;
}

struct file *get_file_pointer(task_t *t, int n)
{
	struct file_ptr *fp = get_file_handle(t, n);
	if(fp && !fp->fi)
		panic(PANIC_NOSYNC, "found empty file handle in task %d pointer list", t->pid);
	return fp ? fp->fi : 0;
}

void remove_file_pointer(task_t *t, int n)
{
	if(!t || !t->filp)
		return;
	struct file_ptr *f = get_file_handle(t, n);
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
	task_critical();
	f->fi->count--;
	task_uncritical();
	if(!f->fi->count)
		kfree(f->fi);
	kfree(f);
}
/* Here we find an unused filedes, and add it to the list. We rely on the list being sorted, 
 * and since this is the only function that adds to it, we can assume it is. This allows
 * for relatively efficient determining of a filedes without limit. */
int add_file_pointer_do(task_t *t, struct file_ptr *f, int after)
{
	if(!t) 
		return -1;
	struct file_ptr *p = t->filp;
	if(!p) {
		t->filp = f;
		f->num=after;
		return f->num;
	}
	if(p->num > after)
	{
		p->prev = f;
		f->next = p;
		f->prev=0;
		t->filp=f;
		f->num=after;
		return f->num;
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
	struct file_ptr *fp = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
	fp->fi = f;
	int r = add_file_pointer_do(t, fp, 0);
	fp->num = r;
	return r;
}

int add_file_pointer_after(task_t *t, struct file *f, int x)
{
	struct file_ptr *fp = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
	fp->fi = f;
	int r = add_file_pointer_do(t, fp, x);
	fp->num = r;
	return r;
}

void copy_file_handles(task_t *p, task_t *n)
{
	if(!p || !n)
		return;
	struct file_ptr *f = p->filp;
	if(!f) {
		n->filp=0;
		return;
	}
	struct file_ptr *new, *prev=0, *start=0;
	start = new = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
	while(f) {
		new->fi = f->fi;
		struct inode *i = f->fi->inode;
		change_icount(i, 1);
		mutex_on(&i->lock);
		i->f_count++;
		if(i->pipe && !i->pipe->type) {
			mutex_on(i->pipe->lock);
			++i->pipe->count;
			if(f->fi->flag & _FWRITE) i->pipe->wrcount++;
			mutex_off(i->pipe->lock);
		}
		mutex_off(&i->lock);
		task_critical();
		new->fi->count++;
		task_uncritical();
		new->num = f->num;
		f=f->next;
		if(f) {
			prev = new;
			new = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
			prev->next = new;
			new->prev = prev;
		}
	}
	n->filp = start;
}
