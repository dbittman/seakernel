#ifndef __SEA_FS_MOUNT_H
#define __SEA_FS_MOUNT_H

#include <sea/fs/inode.h>

#include <types.h>
#include <sys/stat.h>

struct sblktbl {
	int version;
	char name[16];
	struct inode * (*sb_load)(dev_t dev, u64 block, char *);
	struct llistnode *node;
};

struct mountlst {
	struct inode *i;
	struct llistnode *node;
};



extern struct sblktbl *sb_table;
extern struct llist *mountlist;

struct inode *fs_get_filesystem(int _n);
struct mountlst *fs_get_mount(struct inode *i);
int fs_init_superblock_table();
void fs_unmount_all();
void fs_do_sync_of_mounted();
int fs_register_filesystemt(char *name, int ver, int (*sbl)(dev_t,u64,char *));
struct inode *fs_filesystem_callback(char *fsn, dev_t dev, u64 block, char *n);
struct inode *fs_filesystem_check_all(dev_t dev, u64 block, char *n);
int fs_unregister_filesystem(char *name);
int vfs_do_unmount(struct inode *i, int flags);
int vfs_unmount(char *n, int flags);

#endif
