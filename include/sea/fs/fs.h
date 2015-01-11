#ifndef __SEA_FS_FS_H
#define __SEA_FS_FS_H

#include <sea/dm/dev.h>

struct inode;
struct dirent;
struct filesystem;

struct filesystem_callbacks {
	int (*alloc_inode)(struct filesystem *, uint32_t *);
};

struct filesystem_inode_callbacks {
	int (*pull)(struct filesystem *, struct inode *);
	int (*push)(struct filesystem *, struct inode *);
	int (*lookup)(struct filesystem *fs, struct inode *node,
			const char *name, size_t namelen, struct dirent *dir);
	int (*readdir)(struct filesystem *fs, struct inode *node,
			size_t num, struct dirent *dir);
	int (*link)(struct filesystem *fs, struct inode *parent, struct inode *target,
			const char *name, size_t namelen);
	int (*unlink)(struct filesystem *fs, struct inode *parent, const char *name,
			size_t namelen);
	int (*read)(struct filesystem *fs, struct inode *node,
			size_t offset, size_t length, char *buffer);
	int (*write)(struct filesystem *fs, struct inode *node,
			size_t offset, size_t length, const char *buffer);
	int (*select)(struct filesystem *, struct inode *, int rw);
};

struct filesystem {
	uint32_t root_inode_id;
	int id;
	dev_t dev;

	struct filesystem_callbacks       *fs_ops;
	struct filesystem_inode_callbacks *fs_inode_ops;

	void *data;
};

void fs_fssync(struct filesystem *fs);
#endif

