#ifndef __SEA_FS_RAMFS_H
#define __SEA_FS_RAMFS_H

#include <sea/types.h>
#include <sea/fs/inode.h>
struct inode *fs_init_ramfs();
struct inode *fs_init_tmpfs();
int fs_ramfs_write(struct inode *i, off_t off, size_t len, char *b);
int fs_ramfs_read(struct inode *i, off_t off, size_t len, char *b);
struct inode *rfs_create(struct inode *__p, char *name, mode_t mode);

extern struct inode *ramfs_root;

#endif
