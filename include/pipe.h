#ifndef PIPE_H
#define PIPE_H


typedef struct pipe_struct {
	volatile unsigned pending;
	volatile unsigned write_pos, read_pos;
	volatile char *buffer;
	volatile unsigned length;
	mutex_t *lock;
	char type;
	volatile int count, wrcount;
} pipe_t;

int sys_mkfifo(char *path, unsigned mode);
void free_pipe(struct inode *i);
int read_pipe(struct inode *ino, char *buffer, unsigned length);
int write_pipe(struct inode *ino, char *buffer, unsigned length);
int sys_pipe(int *files);
#define PIPE_NAMED 1


#endif
