#ifndef PIPE_H
#define PIPE_H

#include <fs.h>
#include <types.h>
#include <mutex.h>
#include <ll.h>

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
void free_pipe(struct inode *i);
int read_pipe(struct inode *ino, char *buffer, size_t length);
int write_pipe(struct inode *ino, char *buffer, size_t length);
int sys_pipe(int *files);

#endif
