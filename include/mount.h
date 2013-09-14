#ifndef __MOUNT_H
#define __MOUNT_H

#include <types.h>
#include <fs.h>
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

struct mountlst *get_mount(struct inode *i);
int do_fs_stat(struct inode *i, struct fsstat *f);
int fs_stat(char *path, struct fsstat *f);
int sys_fsstat(int fp, struct fsstat *fss);
void add_mountlst(struct inode *n);
void remove_mountlst(struct inode *n);
void unmount_all();
int register_sbt(char *name, int ver, int (*sbl)(dev_t,u64,char *));
struct inode *sb_callback(char *fsn, dev_t dev, u64 block, char *n);
struct inode *sb_check_all(dev_t dev, u64 block, char *n);
int unregister_sbt(char *name);
int sys_posix_fsstat(int fd, struct posix_statfs *sb);
int load_superblocktable();

extern struct sblktbl *sb_table;
extern struct llist *mountlist;

#endif
