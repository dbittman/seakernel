#ifndef _FCNTL_H
#define _FCNTL_H
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/sys/fcntl.h>

//struct flock *create_flock(int type, int whence, off_t start, size_t len);
int can_access_file(struct inode *file, int access);
int fcntl_getlk(struct file *f, long arg);
int fcntl_setlk(struct file *f, long arg);
int fcntl_setlkw(struct file *f, long arg);
void destroy_flocks(struct inode *f);

#endif
