#ifndef __SEA_DM_PIPE_H
#define __SEA_DM_PIPE_H

#include <sea/lib/linkedlist.h>
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>

struct pipe {
	size_t pending;
	size_t write_pos, read_pos;
	char *buffer;
	off_t length;
	struct mutex lock;
	_Atomic int wrcount, recount;
};

int sys_pipe(int *files);

#endif

