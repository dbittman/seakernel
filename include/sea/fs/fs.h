#ifndef __SEA_FS_FS_H
#define __SEA_FS_FS_H

#include <sea/dm/dev.h>
#include <sea/sys/stat.h>
#include <sea/ll.h>

struct dirent_posix;
struct inode;
struct dirent;
struct filesystem;

struct filesystem_callbacks {
	int (*alloc_inode)(struct filesystem *, uint32_t *);
	int (*dealloc_inode)(struct filesystem *, uint32_t);
	int (*fs_stat)(struct filesystem *, struct posix_statfs *);
};

struct filesystem_inode_callbacks {
	int (*pull)(struct filesystem *, struct inode *);
	int (*push)(struct filesystem *, struct inode *);
	int (*lookup)(struct filesystem *fs, struct inode *node,
			const char *name, size_t namelen, struct dirent *dir);
	int (*getdents)(struct filesystem *fs, struct inode *node, unsigned off,
			struct dirent_posix *, unsigned count, unsigned *);
	int (*link)(struct filesystem *fs, struct inode *parent, struct inode *target,
			const char *name, size_t namelen);
	int (*unlink)(struct filesystem *fs, struct inode *parent, const char *name,
			size_t namelen, struct inode *target);
	int (*read)(struct filesystem *fs, struct inode *node,
			size_t offset, size_t length, char *buffer);
	int (*write)(struct filesystem *fs, struct inode *node,
			size_t offset, size_t length, const char *buffer);
	int (*select)(struct filesystem *, struct inode *, int rw);
};

struct fsdriver;

struct filesystem {
	uint32_t root_inode_id;
	int id;
	dev_t dev;
	char type[128];
	int opts;
	struct inode *point;

	struct filesystem_callbacks       *fs_ops;
	struct filesystem_inode_callbacks *fs_inode_ops;

	void *data;
	struct fsdriver *driver;
	struct llistnode *listnode;

	char *pointname;
	char *nodename;
	int usecount;
};

struct fsdriver {
	const char *name;
	int flags;
	int (*mount)(struct filesystem *);
	int (*umount)(struct filesystem *);
	struct llistnode *ln;
};

int fs_fssync(struct filesystem *fs);
void fs_unmount_all();

int fs_callback_inode_read(struct inode *node, size_t off, size_t len, char *buf);
int fs_callback_inode_write(struct inode *node, size_t off, size_t len, const char *buf);
int fs_callback_inode_pull(struct inode *node);
int fs_callback_inode_push(struct inode *node);
int fs_callback_inode_link(struct inode *node, struct inode *target, const char *name, size_t namelen);
int fs_callback_inode_unlink(struct inode *node, const char *name, size_t namelen, struct inode *);
int fs_callback_inode_lookup(struct inode *node, const char *name,
		size_t namelen, struct dirent *dir);
int fs_callback_inode_getdents(struct inode *node, unsigned, struct dirent_posix *, unsigned, unsigned *);
int fs_callback_inode_select(struct inode *node, int rw);
int fs_callback_fs_alloc_inode(struct filesystem *fs, uint32_t *id);
int fs_callback_fs_stat(struct filesystem *fs, struct posix_statfs *p);
int fs_callback_fs_dealloc_inode(struct filesystem *fs, uint32_t *id);
int kerfs_mount_report(size_t offset, size_t length, char *buf);

int ramfs_mount(struct filesystem *fs);
struct filesystem *fs_filesystem_create();
void fs_filesystem_destroy(struct filesystem *fs);
int fs_filesystem_register(struct fsdriver *fd);
int fs_filesystem_unregister(struct fsdriver *fd);
void fs_fsm_init();
int fs_umount(struct filesystem *fs);
int fs_mount(struct inode *pt, struct filesystem *fs);
int fs_filesystem_init_mount(struct filesystem *fs, char *point, char *node, char *type, int opts);
int sys_fs_mount(char *node, char *point, char *type, int opts);
#endif

