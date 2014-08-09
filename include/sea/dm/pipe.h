#ifndef __SEA_DM_PIPE_H
#define __SEA_DM_PIPE_H

#include <sea/ll.h>
#include <sea/mutex.h>
#include <sea/types.h>
#include <sea/fs/inode.h>

#define PIPE_NAMED 1
typedef struct pipe_struct {
	volatile unsigned pending;
	volatile unsigned write_pos, read_pos;
	volatile char *buffer;
	volatile off_t length;
	mutex_t *lock;
	char type;
	volatile int count, wrcount;
	struct llist *read_blocked, *write_blocked;
} pipe_t;

int sys_mkfifo(char *path, mode_t mode);
void dm_free_pipe(struct inode *i);
int dm_read_pipe(struct inode *ino, int flags, char *buffer, size_t length);
int dm_write_pipe(struct inode *ino, int flags, char *buffer, size_t length);
int dm_pipedev_select(struct inode *in, int rw);
int sys_pipe(int *files);
pipe_t *dm_create_pipe();

#endif
