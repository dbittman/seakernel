#ifndef PIPE_H
#define PIPE_H

#include <fs.h>
#include <types.h>
#include <mutex.h>
#include <ll.h>

#include <sea/dm/pipe.h>

int sys_mkfifo(char *path, mode_t mode);
void free_pipe(struct inode *i);
int read_pipe(struct inode *ino, char *buffer, size_t length);
int write_pipe(struct inode *ino, char *buffer, size_t length);
int sys_pipe(int *files);
pipe_t *create_pipe();

#endif
