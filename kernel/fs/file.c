/* Code to keep track of file handles */
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <stdatomic.h>
#include <sea/fs/file.h>
#include <sea/tm/process.h>
#include <sea/fs/pipe.h>
#include <sea/sys/fcntl.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>
#include <sea/fs/dir.h>
static struct file_ptr *get_file_handle(struct process *t, int n)
{
	if(n >= FILP_HASH_LEN) return 0;
	struct file_ptr *f = t->filp[n];
	return f;
}

struct file *fs_get_file_pointer(struct process *t, int n)
{
	mutex_acquire(&t->files_lock);
	struct file_ptr *fp = get_file_handle(t, n);
	if(fp && !fp->fi)
		panic(PANIC_NOSYNC, "found empty file handle in task %d pointer list", t->pid);
	struct file *ret=0;
	if(fp) {
		atomic_fetch_add(&fp->count, 1);
		ret = fp->fi;
		assert(ret);
	}
	mutex_release(&t->files_lock);
	return ret;
}

static void remove_file_pointer(struct process *t, int n)
{
	if(n > FILP_HASH_LEN) return;
	if(!t || !t->filp)
		return;
	struct file_ptr *f = get_file_handle(t, n);
	if(!f)
		return;
	t->filp[n] = 0;
	atomic_fetch_sub(&f->fi->count, 1);
	if(!f->fi->count) {
		kfree(f->fi);
	}
	kfree(f);
}

void fs_fput(struct process *t, int fd, char flags)
{
	mutex_acquire(&t->files_lock);
	struct file_ptr *fp = get_file_handle(t, fd);
	assert(fp);
	atomic_fetch_sub(&fp->count, (flags & FPUT_CLOSE) ? 2 : 1);
	if(!fp->count)
		remove_file_pointer(t, fd);
	mutex_release(&t->files_lock);
}

/* Here we find an unused filedes, and add it to the list. We rely on the 
 * list being sorted, and since this is the only function that adds to it, 
 * we can assume it is. This allows for relatively efficient determining of 
 * a filedes without limit. */
int fs_add_file_pointer_do(struct process *t, struct file_ptr *f, int after)
{
	assert(t && f);
	while(after < FILP_HASH_LEN && t->filp[after])
		after++;
	if(after >= FILP_HASH_LEN) {
		return -1;
	}
	t->filp[after] = f;
	f->num = after;
	return after;
}

int fs_add_file_pointer(struct process *t, struct file *f)
{
	struct file_ptr *fp = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
	mutex_acquire(&t->files_lock);
	fp->fi = f;
	int r = fs_add_file_pointer_do(t, fp, 0);
	if(r >= 0) {
		fp->num = r;
		fp->count = 2; /* once for being open, once for being 
						  used by the function that calls this */
	} else {
		kfree(fp);
	}
	mutex_release(&t->files_lock);
	return r;
}

int fs_add_file_pointer_after(struct process *t, struct file *f, int x)
{
	struct file_ptr *fp = (struct file_ptr *)kmalloc(sizeof(struct file_ptr));
	mutex_acquire(&t->files_lock);
	fp->fi = f;
	int r = fs_add_file_pointer_do(t, fp, x);
	if(r >= 0) {
		fp->num = r;
		fp->count = 2; /* once for being open, once for being
						  used by the function that calls this */
	} else {
		kfree(fp);
	}
	mutex_release(&t->files_lock);
	return r;
}

void fs_copy_file_handles(struct process *p, struct process *n)
{
	if(!p || !n)
		return;
	int c=0;
	while(c < FILP_HASH_LEN) {
		if(p->filp[c]) {
			struct file_ptr *fp = (void *)kmalloc(sizeof(struct file_ptr));
			fp->num = c;
			fp->fi = p->filp[c]->fi;
			fp->count = p->filp[c]->count;
			atomic_fetch_add_explicit(&fp->fi->count, 1, memory_order_relaxed);
			struct inode *i = fp->fi->inode;
			vfs_inode_get(i);
			if(fp->fi->dirent)
				vfs_dirent_acquire(fp->fi->dirent);
			if(i->pipe && !i->pipe->type) {
				atomic_fetch_add_explicit(&i->pipe->count, 1, memory_order_relaxed);
				if(fp->fi->flags & _FWRITE)
					atomic_fetch_add_explicit(&i->pipe->wrcount, 1, memory_order_relaxed);
				tm_blocklist_wakeall(i->pipe->read_blocked);
				tm_blocklist_wakeall(i->pipe->write_blocked);
			}
			n->filp[c] = fp;
		}
		c++;
	}
}

void fs_close_all_files(struct process *t)
{
	int q=0;
	for(;q<FILP_HASH_LEN;q++)
	{
		if(t->filp[q]) {
			sys_close(q);
			assert(!t->filp[q]);
		}
	}
}

