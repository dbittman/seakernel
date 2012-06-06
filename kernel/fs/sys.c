#include <kernel.h>
#include <syscall.h>
#include <isr.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <sys/stat.h>
#include <mod.h>
extern volatile long ticks;
int system_setup=0;
/* This function is called once at the start of the init process initialization.
 * It sets the task fs values to possible and useful things, allowing VFS access.
 * It then starts the device and proc filesystems, and opens up /dev/tty1 on
 * file desciptors 0, 1 and 2 (std[in,out,err]).
 * 
 * Beyond that, it can be called by any task at anytime after the first call as
 * a yield call.
 */
int sys_setup(int a)
{
	if(system_setup)
	{
		task_full_uncritical();
		schedule();
		return 1;
	}
	printk(KERN_MILE, "[kernel]: Setting up environment...");
	current_task->pwd = current_task->root = ramfs_root;
	init_dev_fs();
	init_proc_fs();
	add_inode(procfs_root, kproclist);
	char_rw(OPEN, 3*256+1, 0, 0);
	sys_open("/dev/tty1", O_RDWR);   /* stdin  */
	sys_open("/dev/tty1", O_WRONLY); /* stdout */
	sys_open("/dev/tty1", O_WRONLY); /* stderr */
	current_task->tty=1;
	system_setup=1;
	printk(KERN_MILE, "done (i/o/e=%x [tty1]: ok)\n", 3*256+1);
	return 12;
}

void init_vfs()
{
	load_superblocktable();
}

int sys_seek(int fp, unsigned pos, unsigned whence)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	if(S_ISCHR(f->inode->mode) || S_ISFIFO(f->inode->mode))
		return 0;
	if(whence)
		f->pos = ((whence == SEEK_END) ? f->inode->len+pos : f->pos+pos);
	else
		f->pos=pos;
	return f->pos;
}

int sys_fsync(int f)
{
	struct file *file = get_file_pointer((task_t *)current_task, f);
	if(!file)
		return -EBADF;
	/* We don't actually do any buffering of the files themselves
	 * but we can write out the inode data in case it has changed */
	if(file->inode)
		sync_inode_tofs(file->inode);
	return 0;
}

int sys_link(char *s, char *d)
{
	if(!s || !d) 
		return -EINVAL;
	return link(s, d);
}

int sys_umask(int mode)
{
	int old = current_task->cmask;
	current_task->cmask=mode;
	return old;
}

int sys_chmod(char *path, int mode)
{
	if(!path) return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i) return -ENOENT;
	if(i->uid != current_task->uid && current_task->uid)
	{
		iput(i);
		return -EPERM;
	}
	mutex_on(&i->lock);
	i->mode = (i->mode&~0xFFF) | mode;
	sync_inode_tofs(i);
	iput(i);
	return 0;
}

int sys_chown(char *path, int uid, int gid)
{
	if(!path)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	if(!permissions(i, MAY_WRITE)) {
		iput(i);
		return -EACCES;
	}
	i->uid = uid;
	i->gid = gid;
	sync_inode_tofs(i);
	iput(i);
	return 0;
}

int sys_utime(char *path, unsigned a, unsigned m)
{
	if(!path)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	i->mtime = m ? m : get_epoch_time();
	i->atime = a ? a : get_epoch_time();
	sync_inode_tofs(i);
	iput(i);
	return 0;
}

int sys_getnodestr(char *path, char *node)
{
	if(!path || !node)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	strncpy(node, i->node_str, 128);
	iput(i);
	return 0;
}

int sys_ftruncate(int f, unsigned length)
{
	struct file *file = get_file_pointer((task_t *)current_task, f);
	if(!file || !file->inode)
		return -EBADF;
	file->inode->len = length;
	sync_inode_tofs(file->inode);
	return 0;
}

int sys_mknod(char *path, unsigned mode, unsigned dev)
{
	if(!path) return -EINVAL;
	struct inode *i = cget_idir(path, 0, mode);
	if(!i) return -EACCES;
	mutex_on(&i->lock);
	i->dev = dev;
	i->mode = mode;
	sync_inode_tofs(i);
	iput(i);
	return 0;
}

int sys_readlink(char *_link, char *buf, int nr)
{
	if(!_link || !buf)
		return -EINVAL;
	struct inode *i = lget_idir(_link, 0);
	if(!i)
		return -ENOENT;
	int ret = read_fs(i, 0, nr, buf);
	iput(i);
	return ret;
}

int sys_symlink(char *p2, char *p1)
{
	if(!p2 || !p1)
		return -EINVAL;
	struct inode *inode = get_idir(p1, 0);
	if(!inode) inode = lget_idir(p1, 0);
	if(inode)
	{
		iput(inode);
		return -EEXIST;
	}
	inode = cget_idir(p1, 0, 0x1FF);
	if(!inode)
		return -EACCES;
	mutex_on(&inode->lock);
	inode->mode &= 0x1FF;
	inode->mode |= S_IFLNK;
	inode->len=0;
	mutex_off(&inode->lock);
	int ret=0;
	if((ret=sync_inode_tofs(inode)) < 0) {
		iput(inode);
		return ret;
	}
	if((ret=write_fs(inode, 0, strlen(p2), p2)) < 0) {
		iput(inode);
		return ret;
	}
	iput(inode);
	return 0;
}
#define	F_OK	0
#define	R_OK	4
#define	W_OK	2
#define	X_OK	1
int sys_access(char *path, int mode)
{
	if(!path)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	if(current_task->uid == GOD) {
		iput(i);
		return 0;
	}
	int fail=0;
	if(mode & R_OK)
		fail += (permissions(i, MAY_READ) ? 0 : 1);
	if(mode & W_OK)
		fail += (permissions(i, MAY_WRITE) ? 0 : 1);
	if(mode & X_OK)
		fail += (permissions(i, MAY_EXEC) ? 0 : 1);
	iput(i);
	return (fail ? -EACCES : 0);
}

int select_filedes(int i, int rw)
{
	int ready = 1;
	struct file *file = get_file_pointer((task_t *)current_task, i);
	if(!file || !file->inode)
		return -EBADF;
	struct inode *in = file->inode;
	if(S_ISREG(in->mode) || S_ISDIR(in->mode) || S_ISLNK(in->mode))
		ready = vfs_callback_select(in, rw);
	else if(S_ISCHR(in->mode))
		ready = chardev_select(in, rw);
	else if(S_ISBLK(in->mode))
		ready = blockdev_select(in, rw);
	else if(S_ISFIFO(in->mode))
		ready = pipedev_select(in, rw);
	return ready;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout)
{
	if(nfds < 0)
		return -EINVAL;
	if(!nfds)
		return 0;
	unsigned int wait=0;
	int i;
	if(timeout)
		wait = timeout->tv_sec * 1000 + (timeout->tv_usec/1000);
	long end = wait+ticks;
	int total_set=0, is_ok=0;
	int ret=0;
	while((ticks <= end || !wait || !timeout) && !ret)
	{
		total_set=0;
		for(i=0;i<nfds;++i)
		{
			if(FD_ISSET(i, readfds))
			{
				if(select_filedes(i, READ)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, readfds);
			}
			if(FD_ISSET(i, writefds))
			{
				if(select_filedes(i, WRITE)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, writefds);
			}
			if(FD_ISSET(i, errorfds))
			{
				if(select_filedes(i, OTHER)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, errorfds);
			}
		}
		if(!ret)
			ret = total_set;
		if((!wait && timeout) || is_ok)
			break;
		force_schedule();
		if(got_signal(current_task))
			return -EINTR;
	}
	if(timeout)
	{
		timeout->tv_sec = (end-ticks)/1000;
		timeout->tv_usec = ((end-ticks)%1000)*1000;
		if(ticks >= end) {
			timeout->tv_sec = 0;
			timeout->tv_usec = 0;
			return 0;
		}
	}
	return ret;
}
