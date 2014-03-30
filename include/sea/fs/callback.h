#ifndef __SEA_FS_CALLBACK_H
#define __SEA_FS_CALLBACK_H

#include <sea/fs/inode.h>

int vfs_callback_read (struct inode *i, off_t a, size_t b, char *d);
int vfs_callback_write (struct inode *i, off_t a, size_t b, char *d);
int vfs_callback_select (struct inode *i, unsigned int m);
struct inode *vfs_callback_create (struct inode *i,char *d, mode_t m);
struct inode *vfs_callback_lookup (struct inode *i,char *d);
struct inode *vfs_callback_readdir (struct inode *i, unsigned n);
int vfs_callback_link (struct inode *i, char *d);
int vfs_callback_unlink (struct inode *i);
int vfs_callback_rmdir (struct inode *i);
int vfs_callback_sync_inode (struct inode *i);
int vfs_callback_unmount (struct inode *i, unsigned int n);
int vfs_callback_fsstat (struct inode *i, struct posix_statfs *s);
int vfs_callback_fssync (struct inode *i);
int vfs_callback_update (struct inode *i);

#endif
