/* Code to keep track of file handles */
#include <kernel.h>
#include <task.h>
#include <fs.h>

struct file_ptr *get_file_handle(task_t *t, int n)
{
	if(n >= FILP_HASH_LEN) return 0;
	struct file_ptr *f = t->filp[n];
	return f;
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
	if(n > FILP_HASH_LEN) return;
	if(!t || !t->filp)
		return;
	struct file_ptr *f = get_file_handle(t, n);
	if(!f)
		return;
	t->filp[n] = 0;
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
	assert(t && f);
	while(after < FILP_HASH_LEN && t->filp[after])
		after++;
	if(after >= FILP_HASH_LEN)
		panic(0, "tried to use a file descriptor that was too high");
	t->filp[after] = f;
	f->num = after;
	return after;
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
	int c=0;
	while(c < FILP_HASH_LEN) {
		if(p->filp[c]) {
			struct file_ptr *fp = (void *)kmalloc(sizeof(struct file_ptr));
			fp->num = c;
			fp->fi = p->filp[c]->fi;
			task_critical();
			fp->fi->count++;
			task_uncritical();
			struct inode *i = fp->fi->inode;
			assert(i && i->count && i->f_count && !i->unreal);
			mutex_on(&i->lock);
			i->count++;
			i->f_count++;
			mutex_off(&i->lock);
			if(i->pipe && !i->pipe->type) {
				mutex_on(i->pipe->lock);
				++i->pipe->count;
				if(fp->fi->flags & _FWRITE) i->pipe->wrcount++;
				mutex_off(i->pipe->lock);
			}
			n->filp[c] = fp;
		}
		
		c++;
	}
}

void close_all_files(task_t *t)
{
	int q=0;
	for(;q<FILP_HASH_LEN;q++)
	{
		if(t->filp[q])
			sys_close(q);
	}
}
