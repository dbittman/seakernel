#include <sea/dm/pty.h>
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/lib/charbuffer.h>
#include <sea/kobj.h>
#include <sea/vsprintf.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>

/* TODO: persistance */

static _Atomic int __pty_next_num = 1;

void pty_create(struct inode *inode)
{
	if(!inode->devdata) {
		struct pty *p = kmalloc(sizeof(struct pty));
		charbuffer_create(&p->input, CHARBUFFER_DROP, PTY_IN_BUF_SIZE);
		charbuffer_create(&p->output, 0, PTY_OUT_BUF_SIZE);
		mutex_create(&p->cbuf_lock, 0);
		p->num = atomic_fetch_add(&__pty_next_num, 1);
		inode->devdata = p;
	}
}

void pty_destroy(struct inode *inode)
{
	struct pty *p = inode->devdata;
	if(p) {
		mutex_destroy(&p->cbuf_lock);
		charbuffer_destroy(&p->output);
		charbuffer_destroy(&p->input);
		kfree(p);
	}
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
		if(proc->pty == pty) {
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

/* TODO NONBLOCKING */
size_t pty_read(struct file *file, uint8_t *buffer, size_t length)
{
	struct pty *pty;
	if(MINOR(file->inode->phys_dev))
		pty = file->inode->devdata;
	else
		pty = current_process->pty;
	if(!pty)
		return -EIO;
	size_t r = pty->master == file->inode
		? pty_read_master(pty, buffer, length)
		: pty_read_slave(pty, buffer, length);
	
	return r;
}

size_t pty_write(struct file *file, uint8_t *buffer, size_t length)
{
	struct pty *pty;
	if(MINOR(file->inode->phys_dev))
		pty = file->inode->devdata;
	else
		pty = current_process->pty;
	if(!pty)
		return -EIO;
	size_t r = pty->master == file->inode
		? pty_write_master(pty, buffer, length)
		: pty_write_slave(pty, buffer, length);
	return r;
}

int pty_select(struct file *file, int rw)
{
	struct pty *pty;
	if(MINOR(file->inode->phys_dev))
		pty = file->inode->devdata;
	else
		pty = current_process->pty;
	if(!pty)
		return -EIO;
	if(pty->master == file->inode) {
		if(rw == READ) {
			return charbuffer_count(&pty->output) > 0;
		} else if(rw == WRITE) {
			if(pty->term.c_lflag & ICANON) {
				return pty->cbuf_pos < (PTY_CBUF_SIZE - 1);
			} else {
				return charbuffer_count(&pty->input) < pty->input.cap;
			}
		}
	} else {
		if(rw == READ) {
			return charbuffer_count(&pty->input) > 0;
		} else if(rw == WRITE) {
			return charbuffer_count(&pty->output) < pty->output.cap;
		}
	}
	return 1;
}

int pty_ioctl(struct file *file, int cmd, long arg)
{
	struct pty *pty;
	if(MINOR(file->inode->phys_dev))
		pty = file->inode->devdata;
	else
		pty = current_process->pty;
	if(!pty)
		return -EIO;
	struct termios *term = (void *)arg;
	struct winsize *win  = (void *)arg;
	int ret = 0;
	switch(cmd) {
		case TCGETS:
			if(term)
				memcpy(term, &pty->term, sizeof(*term));
			break;
		case TCSETS: case TCSETSW:
			if(term)
				memcpy(&pty->term, term, sizeof(*term));
			//printk(0, "Setting term %o %o %o\n", term->c_lflag, term->c_iflag, term->c_oflag);
			break;
		case TIOCGWINSZ:
			if(win)
				memcpy(win, &pty->size, sizeof(*win));
			break;
		case TIOCSWINSZ:
			if(win)
				memcpy(&pty->size, win, sizeof(*win));
			break;
		default:
			printk(0, "[pty]: unknown ioctl: %x\n", cmd);
			return -EINVAL;
	}
	return ret;
}

static ssize_t __pty_rw(int rw, struct file *file, off_t off, uint8_t *buf, size_t len)
{
	if(rw == READ)
		return pty_read(file, buf, len);
	else if(rw == WRITE)
		return pty_write(file, buf, len);
	return -EIO;
}

struct kdevice __pty_kdev = {
	.rw = __pty_rw,
	.select = pty_select,
	.ioctl = pty_ioctl,
	.create = pty_create,
	.destroy = pty_destroy,
	.open = 0, //TODO: should set controlling terminal
	.close = 0,
	.name = "pty",
};

static int pty_major;

void pty_init(void)
{
	pty_major = dm_device_register(&__pty_kdev);
	sys_mknod("/dev/tty", S_IFCHR | 0666, GETDEV(pty_major, 0));
}

int sys_openpty(int *master, int *slave, char *slavename, const struct termios *term,
		const struct winsize *win)
{
	int num = atomic_fetch_add(&__pty_next_num, 1);
	
	char mname[32];
	char sname[32];
	snprintf(mname, 32, "/dev/ptym%d", num);
	snprintf(sname, 32, "/dev/ptys%d", num);

	sys_mknod(mname, S_IFCHR | 0666, GETDEV(pty_major, num));
	sys_mknod(sname, S_IFCHR | 0666, GETDEV(pty_major, num));

	int mfd = sys_open(mname, O_RDWR, 0);
	int sfd = sys_open(sname, O_RDWR, 0);
	if(mfd < 0 || sfd < 0) {
		sys_unlink(mname);
		sys_unlink(sname);
		return -ENOENT;
	}

	struct file *mf = file_get(mfd);
	struct file *sf = file_get(sfd);
	vfs_inode_get(mf->inode);
	vfs_inode_get(sf->inode);
	
	/* TODO: better system */
	pty_create(mf->inode);
	sf->inode->devdata = mf->inode->devdata;
	struct pty *pty = mf->inode->devdata;
	assert(pty);
	
	pty->master = mf->inode;
	pty->slave = sf->inode;
	if(term)
		memcpy(&pty->term, term, sizeof(*term));
	if(win)
		memcpy(&pty->size, win, sizeof(*win));
	file_put(mf);
	file_put(sf);

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
	struct file *mf = file_get(fd);
	if(!mf) {
		return -EBADF;
	}
	if(MAJOR(mf->inode->phys_dev) != pty_major) {
		file_put(mf);
		return -EINVAL;
	}
	struct pty *pty = mf->inode->devdata;
	current_process->pty = pty;
	file_put(mf);
	return 0;
}

