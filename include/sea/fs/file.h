#ifndef __SEA_FS_FILE_H
#define __SEA_FS_FILE_H

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

#include <types.h>
#include <sea/tm/process.h>



struct file {
	unsigned int flags, fd_flags, count;
	off_t pos;
	struct inode * inode;
};

struct file_ptr {
	unsigned int num, count;
	struct file *fi;
};

struct file *fs_get_file_pointer(task_t *t, int n);
void fs_fput(task_t *t, int fd, char flags);
int fs_add_file_pointer(task_t *t, struct file *f);
int fs_add_file_pointer_after(task_t *t, struct file *f, int x);
void fs_copy_file_handles(task_t *p, task_t *n);
void fs_close_all_files(task_t *t);

#endif

