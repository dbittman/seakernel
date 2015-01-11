/* Defines kernel-space side of functions for dealing with file locks. 
 * This should be compatible with POSIX (or, mostly so).
 */
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/fs/fcntl.h>
#include <sea/tm/schedule.h>
#include <sea/fs/file.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>
#define LSTART(a) (a->l_start + a->l_pos)
void vfs_init_inode_flocks(struct inode *i)
{
	if(!i->flm)
		i->flm = mutex_create(0, 0);
}

static struct flock *create_flock(int type, int whence, off_t start, size_t len)
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

static struct flock *get_flock_blocker(struct inode *file, struct flock *l, int pos)
{
	if(!file || !l) return 0;
	switch(l->l_whence)
	{
		case SEEK_SET:
			l->l_pos=0;
			break;
		case SEEK_CUR:
			l->l_pos = pos;
			break;
		case SEEK_END:
			l->l_pos = file->length;
			break;
	}
	struct flock *cur = file->flocks;
	while(cur)
	{
		int q = is_overlap(l, cur);
		if(q) {
			if(l->l_type == F_WRLCK || cur->l_type == F_WRLCK) {
				return cur;
			}
		}
		cur = cur->next;
	}
	
	return 0;
}

static int can_flock(struct inode *file, struct flock *l)
{
	if(!file || !l) return 0;
	
	struct flock *cur = file->flocks;
	while(cur)
	{
		if(is_overlap(l, cur))
			if(l->l_type == F_WRLCK || cur->l_type == F_WRLCK) {
				return 0;
			}
		cur = cur->next;
	}
	return 1;
}

static int disengage_flock(struct inode *file, struct flock *l)
{
	if(!file || !l) return -EINVAL;
	if(l->prev)
		l->prev->next = l->next;
	if(l->next)
		l->next->prev = l->prev;
	if(file->flocks == l)
		file->flocks = l->next ? l->next : l->prev;
	kfree(l);
	return 0;
}

static int engage_flock(struct inode *inode, struct flock *l, int pos)
{
	if(!inode || !l) return -EINVAL;
	
	if(!vfs_inode_check_permissions(inode, (l->l_type == F_WRLCK) ? MAY_WRITE : MAY_READ, 0))
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
			l->l_pos = inode->length;
			break;
	}
	
	if(LSTART(l) < 0)
		return -EINVAL;
	
	struct flock *old = inode->flocks;
	inode->flocks = l;
	l->next = old;
	inode->flocks->prev = l;
	l->prev=0;

	return 0;
}

static struct flock *find_flock(struct inode *f, struct flock *l)
{
	assert(f && l);
	struct flock *c = f->flocks;
	while(c)
	{
		if(l->l_start == c->l_start && l->l_whence == c->l_whence 
				&& l->l_len == c->l_len && l->l_pid == c->l_pid) {
			return c;
		}
		c=c->next;
	}
	return 0;
}

int fs_fcntl_setlk(struct file *file, long arg)
{
	assert(file);
	printk(1, "[untested]: Flock - setlk\n");
	if(!arg || !file) return -EINVAL;
	vfs_init_inode_flocks(file->inode);
	struct inode *f = file->inode;
	struct flock *p = (struct flock *)arg;
	mutex_acquire(f->flm);
	if(p->l_type == F_UNLCK)
	{
		p->l_pid = sys_get_pid();
		p = find_flock(f, p);
		if(!p) {
			mutex_release(f->flm);
			return -EAGAIN;
		}
		int x = disengage_flock(f, p);
		mutex_release(f->flm);
		return x;
	}
	p = create_flock(p->l_type, p->l_whence, p->l_start, p->l_len);
	int ret = engage_flock(f, p, file->pos);
	mutex_release(f->flm);
	if(ret < 0)
		kfree(p);
	return ret;
}

int fs_fcntl_getlk(struct file *file, long arg)
{
	assert(file);
	printk(1, "[untested]: Flock - getlk\n");
	struct inode *f = file->inode;
	vfs_init_inode_flocks(f);
	if(!arg) return -EINVAL;
	mutex_acquire(f->flm);
	struct flock *l = get_flock_blocker(f, (struct flock *)arg, file->pos);
	if(!l)
		((struct flock *)arg)->l_type = F_UNLCK;
	else
		memcpy(((struct flock *)arg), l, sizeof(struct flock));
	mutex_release(f->flm);
	return 0;
}

int fs_fcntl_setlkw(struct file *file, long arg)
{
	assert(file);
	vfs_init_inode_flocks(file->inode);
	printk(1, "[untested]: Flock - setlkw\n");
	struct inode *f = file->inode;
	int ret;
	while(1) {
		if(!(ret=fs_fcntl_setlk(file, arg)))
			return 0;
		if(ret != -EAGAIN)
			return -EACCES;
		tm_schedule();
		if(tm_process_got_signal(current_task))
			return -EINTR;
	}
}

void vfs_destroy_flocks(struct inode *f)
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
	if(f->flm) 
		mutex_destroy(f->flm);
}
