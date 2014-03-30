#ifndef __SEA_FS_DEVFS_H
#define __SEA_FS_DEVFS_H

#include <sea/types.h>
#include <sea/fs/inode.h>

void devfs_init();
struct inode *devfs_add(struct inode *q, char *name, mode_t mode, int major, int minor);
struct inode *devfs_create(struct inode *base, char *name, mode_t mode);
void devfs_remove(struct inode *i);
int devfs_fsstat(struct inode *i, struct posix_statfs *fs);
extern struct inode *devfs_root;

#endif
