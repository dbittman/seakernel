#ifndef __SEA_DM_PTY_H
#define __SEA_DM_PTY_H

#include <sea/lib/charbuffer.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/sys/termios.h>
#include <sea/mutex.h>

#define PTY_IN_BUF_SIZE 1024
#define PTY_OUT_BUF_SIZE 1024
#define PTY_CBUF_SIZE 256

struct pty {
	int num;
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

int sys_openpty(int *master, int *slave, char *name, const struct termios *term,
		const struct winsize *win);
int sys_attach_pty(int fd);
void pty_init(void);

#endif

