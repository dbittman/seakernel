/* Defines kernel-space side of functions for dealing with file locks. This should be compatible with POSIX
 * (or, mostly so).
 */
#include <kernel.h>
#include <task.h>
#include <fs.h>
#include <fcntl.h>
#define LSTART(a) (a->l_start + a->l_pos)

void init_flocks(struct inode *i)
{
	if(!i->flm)
		i->flm = create_mutex(0);
}

struct flock *create_flock(int type, int whence, int start, int len)
{
	struct flock *fl = (struct flock *)kmalloc(sizeof(struct flock));
	fl->l_type = type;
	fl->l_whence = whence;
	fl->l_start = start;
	fl->l_len = len;
	fl->l_pid = current_task->pid;
	return fl;
}

static int is_overlap(struct flock *a, struct flock *b)
{
	if(LSTART(a) + a->l_len > LSTART(b))
		if(LSTART(b) + b->l_len > LSTART(a))
			return 1;
	return 0;
}

struct flock *get_flock_blocker(struct inode *file, struct flock *l, int pos)
{
	if(!file || !l) return 0;
	mutex_on(file->flm);
	switch(l->l_whence)
	{
		case SEEK_SET:
			l->l_pos=0;
			break;
		case SEEK_CUR:
			l->l_pos = pos;
			break;
		case SEEK_END:
			l->l_pos = file->len;
			break;
	}
	struct flock *cur = file->flocks;
	while(cur)
	{
		int q = is_overlap(l, cur);
		if(q) {
			if(l->l_type == F_WRLCK || cur->l_type == F_WRLCK) {
				mutex_off(file->flm);
				return cur;
			}
		}
		cur = cur->next;
	}
	
	mutex_off(file->flm);
	return 0;
}

int can_flock(struct inode *file, struct flock *l)
{
	if(!file || !l) return 0;
	mutex_on(file->flm);
	
	struct flock *cur = file->flocks;
	while(cur)
	{
		if(is_overlap(l, cur))
			if(l->l_type == F_WRLCK || cur->l_type == F_WRLCK) {
				mutex_off(file->flm);
				return 0;
			}
		cur = cur->next;
	}
	
	mutex_off(file->flm);
	return 1;
}

int disengage_flock(struct inode *file, struct flock *l)
{
	if(!file || !l) return -EINVAL;
	mutex_on(file->flm);
	if(l->prev)
		l->prev->next = l->next;
	if(l->next)
		l->next->prev = l->prev;
	if(file->flocks == l)
		file->flocks = l->next ? l->next : l->prev;
	kfree(l);
	mutex_off(file->flm);
	return 0;
}

int engage_flock(struct inode *inode, struct flock *l, int pos)
{
	if(!inode || !l) return -EINVAL;
	
	if(!permissions(inode, (l->l_type == F_WRLCK) ? MAY_WRITE : MAY_READ))
		return -EACCES;
	
	if(!can_flock(inode, l))
		return -EAGAIN;
	
	switch(l->l_whence)
	{
		case SEEK_SET:
			l->l_pos=0;
			break;
		case SEEK_CUR:
			l->l_pos = pos;
			break;
		case SEEK_END:
			l->l_pos = inode->len;
			break;
	}
	
	if(LSTART(l) < 0)
		return -EINVAL;
	
	mutex_on(inode->flm);
	struct flock *old = inode->flocks;
	inode->flocks = l;
	l->next = old;
	inode->flocks->prev = l;
	l->prev=0;
	mutex_off(inode->flm);
	
	return 0;
}

struct flock *find_flock(struct inode *f, struct flock *l)
{
	assert(f && l);
	struct flock *c = f->flocks;
	mutex_on(f->flm);
	while(c)
	{
		if(l->l_start == c->l_start && l->l_whence == c->l_whence && l->l_len == c->l_len && l->l_pid == c->l_pid) {
			mutex_off(f->flm);
			return c;
		}
		c=c->next;
	}
	mutex_off(f->flm);
	return 0;
}

int fcntl_setlk(struct file *file, int arg)
{
	assert(file);
	printk(1, "[untested]: Flock - setlk\n");
	if(!arg || !file) return -EINVAL;
	init_flocks(file->inode);
	struct inode *f = file->inode;
	struct flock *p = (struct flock *)arg;
	if(p->l_type == F_UNLCK)
	{
		p->l_pid = get_pid();
		p = find_flock(f, p);
		if(!p)
			return -EAGAIN;
		return disengage_flock(f, p);
	}
	p = create_flock(p->l_type, p->l_whence, p->l_start, p->l_len);
	return engage_flock(f, p, file->pos);
}

int fcntl_getlk(struct file *file, int arg)
{
	assert(file);
	printk(1, "[untested]: Flock - getlk\n");
	struct inode *f = file->inode;
	init_flocks(f);
	if(!arg) return -EINVAL;
	mutex_on(f->flm);
	struct flock *l = get_flock_blocker(f, (struct flock *)arg, file->pos);
	if(!l)
		((struct flock *)arg)->l_type = F_UNLCK;
	else
		memcpy(((struct flock *)arg), l, sizeof(struct flock));
	mutex_off(f->flm);
	return 0;
}

int fcntl_setlkw(struct file *file, int arg)
{
	assert(file);
	init_flocks(file->inode);
	printk(1, "[untested]: Flock - setlkw\n");
	struct inode *f = file->inode;
	int ret;
	while(1) {
		if(!(ret=fcntl_setlk(file, arg)))
			return 0;
		if(ret != -EAGAIN)
			return -EACCES;
		f->newlocks=0;
		wait_flag_except((unsigned *)&f->newlocks, 0);
		if(got_signal(current_task))
			return -EINTR;
	}
}

void destroy_flocks(struct inode *f)
{
	if(!f)
		return;
	struct flock *l = f->flocks;
	while(l)
	{
		struct flock *tok = l;
		l=l->next;
		kfree(tok);
	}
	if(f->flm) {
		f->flm->pid=-1;
		destroy_mutex(f->flm);
	}
}
