/* Code to keep track of file handles */
#include <sea/tm/blocking.h>
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
#include <sea/lib/bitmap.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>
static int __allocate_fdnum(int start)
{
	int ent = bitmap_ffr_start(current_process->fdnum_bitmap, NUM_FD, start);
	if(ent != -1)
		bitmap_set(current_process->fdnum_bitmap, ent);
	return ent;
}

static void __free_fdnum(int num)
{
	bitmap_reset(current_process->fdnum_bitmap, num);
}

struct file *file_get_ref(struct file *file)
{
	atomic_fetch_add(&file->count, 1);
	return file;
}

struct file *file_create(struct inode *inode, struct dirent *dir,
		int flags)
{
	struct file *file = kmalloc(sizeof(struct file));
	vfs_inode_get(inode);
	if(dir)
		atomic_fetch_add(&dir->count, 1);
	file->flags = flags;
	file->inode = inode;
	file->dirent = dir;
	file->count = ATOMIC_VAR_INIT(1);
	if(inode->kdev) {
	printk(0, "ok: %x\n", &inode->kdev);
		//inode->kdev->open(file);
	}
	return file;
}

void file_put(struct file *file)
{
	if(atomic_fetch_sub(&file->count, 1) == 1) {
		/* destroy */
		if(file->inode->kdev && file->inode->kdev->close)
			file->inode->kdev->close(file);
		if(file->dirent)
			vfs_dirent_release(file->dirent);
		vfs_icache_put(file->inode);
		kfree(file);
	}
}

struct file *file_get(int fdnum)
{
	mutex_acquire(&current_process->fdlock);
	struct filedes *fd = hash_lookup(&current_process->files, &fdnum, sizeof(fdnum));
	struct file *file = NULL;
	if(fd)
		file = file_get_ref(fd->file);
	mutex_release(&current_process->fdlock);
	return file;
}

int file_add_filedes(struct file *f, int start)
{
	mutex_acquire(&current_process->fdlock);
	int num = __allocate_fdnum(start);
	if(num == -1) {
		mutex_release(&current_process->fdlock);
		return -1;
	}
	struct filedes *des = kmalloc(sizeof(struct filedes));
	des->num = num;
	des->file = file_get_ref(f);
	hash_insert(&current_process->files, &des->num, sizeof(des->num), &des->elem, des);
	mutex_release(&current_process->fdlock);
	return num;
}

static void __file_remove_filedes(struct filedes *f)
{
	__free_fdnum(f->num);
	hash_delete(&current_process->files, &f->num, sizeof(f->num));
	struct file *file = f->file;
	kfree(f);
	file_put(file);
}

void file_remove_filedes(struct filedes *f)
{
	mutex_acquire(&current_process->fdlock);
	__file_remove_filedes(f);
	mutex_release(&current_process->fdlock);
}

int file_close_fd(int fd)
{
	mutex_acquire(&current_process->fdlock);
	struct filedes *des = hash_lookup(&current_process->files, &fd, sizeof(fd));
	if(des)
		__file_remove_filedes(des);
	mutex_release(&current_process->fdlock);
	return des == NULL ? -EBADF : 0;
}

void fs_copy_file_handles(struct process *p, struct process *n)
{
	assert(p && n);
	for(int i=0;i<NUM_FD;i++) {
		struct file *file = file_get(i);
		if(file) {
			bitmap_set(&n->fdnum_bitmap, i);
			struct filedes *des = kmalloc(sizeof(struct filedes));
			des->num = i;
			des->file = file_get_ref(file);
			hash_insert(&n->files, &des->num, sizeof(des->num), &des->elem, des);
			file_put(file);
		}
	}
}

static void __files_map_close(struct hashelem *elem)
{
	__file_remove_filedes(elem->ptr);
}

void file_close_all(void)
{
	mutex_acquire(&current_process->fdlock);
	hash_map(&current_process->files, __files_map_close);
	mutex_release(&current_process->fdlock);
}

