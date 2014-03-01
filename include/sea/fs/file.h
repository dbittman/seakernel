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

struct file *fs_do_sys_open(char *name, int flags, mode_t _mode, int *error, int *num);

int fs_do_sys_read_flags(struct file *f, off_t off, char *buf, size_t count);
int fs_do_sys_read(struct file *f, off_t off, char *buf, size_t count);
int sys_read(int fp, off_t off, char *buf, size_t count);
int sys_readpos(int fp, char *buf, size_t count);
int fs_do_sys_write_flags(struct file *f, off_t off, char *buf, size_t count);
int fs_do_sys_write(struct file *f, off_t off, char *buf, size_t count);
int sys_writepos(int fp, char *buf, size_t count);
int sys_write(int fp, off_t off, char *buf, size_t count);
int fs_read_file_data(int fp, char *buf, off_t off, size_t length);

#endif

