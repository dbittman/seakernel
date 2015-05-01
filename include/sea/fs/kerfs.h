#ifndef __SEA_FS_KERFS
#define __SEA_FS_KERFS

#include <sea/fs/inode.h>

#define KERFS_TYPE_INTEGER 1

#define KERFS_PARAM 1
#define KERFS_PARAM_READONLY 2

void kerfs_init();
int kerfs_read(struct inode *node, size_t offset, size_t length, char *buffer);
int kerfs_write(struct inode *node, size_t offset, size_t length, const char *buffer);
int kerfs_register_parameter(char *path, void *param, size_t size, int flags, int type);
int kerfs_register_report(char *path, int (*fn)(size_t, size_t, char *));
int kerfs_syscall_report(size_t offset, size_t length, char *buf);
int kerfs_int_report(size_t offset, size_t length, char *buf);
int kerfs_kmalloc_report(size_t offset, size_t length, char *buf);
int kerfs_pmm_report(size_t offset, size_t length, char *buf);

#endif

