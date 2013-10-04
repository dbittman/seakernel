#ifndef __FILE_H
#define __FILE_H

#include <types.h>

#define FILP_HASH_LEN 512

struct file {
	unsigned int flags, fd_flags, count;
	off_t pos;
	struct inode * inode;
};

struct file_ptr {
	unsigned int num, count;
	struct file *fi;
};

struct file *d_sys_open(char *name, int flags, mode_t mode, int *, int *);
int do_sys_write_flags(struct file *f, off_t off, char *buf, size_t count);
int do_sys_read_flags(struct file *f, off_t off, char *buf, size_t count);

#endif
