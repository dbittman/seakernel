#ifndef __SEA_DM_PTY_H
#define __SEA_DM_PTY_H

#include <sea/lib/charbuffer.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/tty/termios.h>
#include <sea/mutex.h>

#define PTY_ALLOC 1

#define PTY_IN_BUF_SIZE 1024
#define PTY_OUT_BUF_SIZE 1024

#define PTY_CBUF_SIZE 256

struct pty {
	int num;
	int flags;
	struct charbuffer input, output;

	unsigned char cbuf[PTY_CBUF_SIZE];
	int cbuf_pos;
	struct mutex cbuf_lock;

	struct inode *master, *slave;
	struct winsize size;
	struct termios term;
	int ldmode;
	struct process *controller;
};

size_t pty_read(struct inode *inode, uint8_t *buffer, size_t length);
size_t pty_write(struct inode *inode, uint8_t *buffer, size_t length);
int pty_select(struct inode *inode, int rw);
int pty_ioctl(struct inode *inode, int cmd, long arg);

int sys_openpty(int *master, int *slave, char *name, const struct termios *term,
		const struct winsize *win);
int sys_attach_pty(int fd);

#endif

