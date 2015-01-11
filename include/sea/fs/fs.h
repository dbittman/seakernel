#ifndef __SEA_FS_FS_H
#define __SEA_FS_FS_H

#include <sea/dm/dev.h>

struct filesystem;

struct filesystem_callbacks {
	int (*alloc_inode)(struct filesystem *);
};

struct filesystem {
	uint32_t root_inode_id;
	int id;
	dev_t dev;

	void *data;
};

void fs_fssync(struct filesystem *fs);
#endif

