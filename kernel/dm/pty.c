#include <sea/dm/pty.h>
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/lib/charbuffer.h>
#include <sea/kobj.h>
#include <sea/vsprintf.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>

static _Atomic int __pty_next_num = 1;

struct pty *pty_create(struct pty *p, int flags)
{
	KOBJ_CREATE(p, flags, PTY_ALLOC);
	charbuffer_create(&p->input, CHARBUFFER_DROP, PTY_IN_BUF_SIZE);
	charbuffer_create(&p->output, 0, PTY_OUT_BUF_SIZE);
	mutex_create(&p->cbuf_lock, 0);
	p->num = atomic_fetch_add(&__pty_next_num, 1);
	return p;
}

void pty_destroy(struct pty *p)
{
	mutex_destroy(&p->cbuf_lock);
	charbuffer_destroy(&p->output);
	charbuffer_destroy(&p->input);
	KOBJ_DESTROY(p, PTY_ALLOC);
}

size_t pty_read_master(struct pty *pty, uint8_t *buffer, size_t length)
{
	size_t r = charbuffer_read(&pty->output, buffer, length);
	return r;
}

static void write_char(struct pty *pty, uint8_t c)
{
	if(c == '\n' && (pty->term.c_oflag & ONLCR)) {
		char d = '\r';
		charbuffer_trywrite(&pty->output, &d, 1);
	}
	charbuffer_trywrite(&pty->output, &c, 1);
}

static void __raise_action(struct pty *pty, int sig)
{
	__linkedlist_lock(process_list);
	struct process *proc;
	struct linkedentry *node;
	for(node = linkedlist_iter_start(process_list); node != linkedlist_iter_end(process_list);
			node = linkedlist_iter_next(node)) {
		proc = linkedentry_obj(node);
		if(proc->tty == pty->num) {
			tm_signal_send_process(proc, sig);
		}
	}
	__linkedlist_unlock(process_list);
}

static void process_input(struct pty *pty, uint8_t c)
{
	if(pty->cbuf_pos < (PTY_CBUF_SIZE - 1)) {
		if(c == pty->term.c_cc[VINTR]) {
			__raise_action(pty, SIGINT);
			if(pty->term.c_lflag & ECHO) {
				write_char(pty, '^');
				write_char(pty, 'C');
				write_char(pty, '\n');
			}
		} else if(c == pty->term.c_cc[VERASE]) {
			if(pty->cbuf_pos > 0) {
				pty->cbuf[pty->cbuf_pos--] = 0;
				if(pty->term.c_lflag & ECHO) {
					write_char(pty, '\b');
					write_char(pty, ' ');
					write_char(pty, '\b');
				}
			}
		} else if(c == pty->term.c_cc[VSUSP]) {
			__raise_action(pty, SIGTSTP);
			if(pty->term.c_lflag & ECHO) {
				write_char(pty, '^');
				write_char(pty, 'Z');
				write_char(pty, '\n');
			}
		} else if(c == pty->term.c_cc[VEOF]) {
			if(pty->cbuf_pos > 0) {
				charbuffer_write(&pty->input, pty->cbuf, pty->cbuf_pos);
				pty->cbuf_pos = 0;
			} else {
				pty->input.eof = 1;
				tm_blocklist_wakeall(&pty->input.readers);
			}
		} else {
			if(c == 27) /* escape */
				c = '^';
			pty->cbuf[pty->cbuf_pos++] = c;
			if(pty->term.c_lflag & ECHO)
				write_char(pty, c);
			if(c == '\n') {
				charbuffer_write(&pty->input, pty->cbuf, pty->cbuf_pos);
				pty->cbuf_pos = 0;
			}
		}
	}
}

size_t pty_write_master(struct pty *pty, uint8_t *buffer, size_t length)
{
	if(pty->term.c_lflag & ICANON) {
		mutex_acquire(&pty->cbuf_lock);
		for(size_t i = 0;i<length;i++) {
			process_input(pty, *buffer++);
		}
		mutex_release(&pty->cbuf_lock);
		return length;
	} else {
		if(pty->term.c_lflag & ECHO)
			charbuffer_write(&pty->output, buffer, length);
		return charbuffer_write(&pty->input, buffer, length);
	}
}

size_t pty_read_slave(struct pty *pty, uint8_t *buffer, size_t length)
{
	return charbuffer_read(&pty->input, buffer, length);
}

size_t pty_write_slave(struct pty *pty, uint8_t *buffer, size_t length)
{
	for(size_t i=0;i<length;i++) {
		if(*buffer == '\n' && (pty->term.c_oflag & ONLCR)) {
			charbuffer_write(&pty->output, (uint8_t *)"\r", 1);
		}
		charbuffer_write(&pty->output, buffer++, 1);
	}
	return length;
}

size_t pty_read(struct inode *inode, uint8_t *buffer, size_t length)
{
	assert(inode->pty);
	size_t r = inode->pty->master == inode
		? pty_read_master(inode->pty, buffer, length)
		: pty_read_slave(inode->pty, buffer, length);
	
	return r;
}

size_t pty_write(struct inode *inode, uint8_t *buffer, size_t length)
{
	assert(inode->pty);
	size_t r = inode->pty->master == inode
		? pty_write_master(inode->pty, buffer, length)
		: pty_write_slave(inode->pty, buffer, length);
	return r;
}

int pty_select(struct inode *inode, int rw)
{
	assert(inode->pty);
	if(inode->pty->master == inode) {
		if(rw == READ) {
			return charbuffer_count(&inode->pty->output) > 0;
		} else if(rw == WRITE) {
			if(inode->pty->term.c_lflag & ICANON) {
				return inode->pty->cbuf_pos < (PTY_CBUF_SIZE - 1);
			} else {
				return charbuffer_count(&inode->pty->input) < inode->pty->input.cap;
			}
		}
	} else {
		if(rw == READ) {
			return charbuffer_count(&inode->pty->input) > 0;
		} else if(rw == WRITE) {
			return charbuffer_count(&inode->pty->output) < inode->pty->output.cap;
		}
	}
	return 1;
}

int pty_ioctl(struct inode *inode, int cmd, long arg)
{
	struct termios *term = (void *)arg;
	struct winsize *win  =(void *)arg;
	int ret = 0;
	switch(cmd) {
		case TCGETS:
			if(term)
				memcpy(term, &inode->pty->term, sizeof(*term));
			break;
		case TCSETS: case TCSETSW:
			if(term)
				memcpy(&inode->pty->term, term, sizeof(*term));
			//printk(0, "Setting term %o %o %o\n", term->c_lflag, term->c_iflag, term->c_oflag);
			break;
		case TIOCGWINSZ:
			if(win)
				memcpy(win, &inode->pty->size, sizeof(*win));
			break;
		case TIOCSWINSZ:
			if(win)
				memcpy(&inode->pty->size, win, sizeof(*win));
			break;
		default:
			printk(0, "[pty]: unknown ioctl: %x\n", cmd);
			return -EINVAL;
	}
	return ret;
}
int ptys = 0; /* TODO: this is a hack to get the old tty system
				 to be okay with ptys. Remove it. */
int sys_openpty(int *master, int *slave, char *slavename, const struct termios *term,
		const struct winsize *win)
{
	ptys = 1;
	struct pty *pty = pty_create(0, 0);
	if(term)
		memcpy(&pty->term, term, sizeof(*term));
	if(win)
		memcpy(&pty->size, win, sizeof(*win));
	
	char mname[32];
	char sname[32];
	snprintf(mname, 32, "/dev/ptym%d", pty->num);
	snprintf(sname, 32, "/dev/ptys%d", pty->num);

	sys_mknod(mname, S_IFCHR | 0666, 0);
	sys_mknod(sname, S_IFCHR | 0666, 0);

	int mfd = sys_open(mname, O_RDWR);
	int sfd = sys_open(sname, O_RDWR);
	if(mfd < 0 || sfd < 0) {
		pty_destroy(pty);
		sys_unlink(mname);
		sys_unlink(sname);
		return -ENOENT;
	}

	struct file *mf = fs_get_file_pointer(current_process, mfd);
	struct file *sf = fs_get_file_pointer(current_process, sfd);
	mf->inode->pty = pty;
	sf->inode->pty = pty;
	vfs_inode_get(mf->inode);
	vfs_inode_get(sf->inode);
	pty->master = mf->inode;
	pty->slave = sf->inode;
	fs_fput(current_process, mfd, 0);
	fs_fput(current_process, sfd, 0);

	if(slavename)
		strncpy(slavename, sname, 32);
	if(master)
		*master = mfd;
	if(slave)
		*slave = sfd;
	return 0;
}

int sys_attach_pty(int fd)
{
	struct file *mf = fs_get_file_pointer(current_process, fd);
	if(!mf) {
		return -EBADF;
	}
	if(!mf->inode->pty) {
		fs_fput(current_process, fd, 0);
		return -EINVAL;
	}
	current_process->tty = mf->inode->pty->num;
	fs_fput(current_process, fd, 0);
	return 0;
}

