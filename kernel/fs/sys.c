#include <kernel.h>
#include <syscall.h>
#include <isr.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <sys/stat.h>
#include <char.h>
#include <atomic.h>
#include <rwlock.h>
#include <symbol.h>
#include <mount.h>
#include <file.h>

int system_setup=0;
/* This function is called once at the start of the init process initialization.
 * It sets the task fs values to possible and useful things, allowing VFS access.
 * It then starts the device and proc filesystems, and opens up /dev/tty1 on
 * file desciptors 0, 1 and 2 (std[in,out,err]).
 * 
 * Beyond that, it can be called by any task at anytime after the first call as
 * a yield call.
 */
int proc_set_callback(int major, int( *callback)(char rw, struct inode *inode, 
	int m, char *buf, int, int));
int sys_setup(int a)
{
	if(system_setup)
	{
		schedule();
		return 1;
	}
	printk(KERN_MILE, "[kernel]: Setting up environment...");
	current_task->thread->pwd = current_task->thread->root = ramfs_root;
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
#if CONFIG_MODULES
	add_kernel_symbol(do_iremove);
	add_kernel_symbol(pfs_cn_node);
	add_kernel_symbol(pfs_cn);
	add_kernel_symbol(sys_open);
	add_kernel_symbol(sys_read);
	add_kernel_symbol(set_as_kernel_task);
	add_kernel_symbol(sys_write);
	add_kernel_symbol(sys_close);
	add_kernel_symbol(read_fs);
	add_kernel_symbol(write_fs);
	add_kernel_symbol(sys_ioctl);
	add_kernel_symbol(proc_append_buffer);
	add_kernel_symbol(sys_stat);
	add_kernel_symbol(sys_fstat);
	add_kernel_symbol(register_sbt);
	add_kernel_symbol(unregister_sbt);
	add_kernel_symbol(iput);
	_add_kernel_symbol((addr_t)(struct inode **)&devfs_root, "devfs_root");
	add_kernel_symbol(do_get_idir);
	add_kernel_symbol(proc_set_callback);
	add_kernel_symbol(proc_get_major);
#endif
}

int sys_seek(int fp, off_t pos, unsigned whence)
{
	struct file *f = get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	if(S_ISCHR(f->inode->mode) || S_ISFIFO(f->inode->mode)) {
		fput((task_t *)current_task, fp, 0);
		return 0;
	}
	if(whence)
		f->pos = ((whence == SEEK_END) ? f->inode->len+pos : f->pos+pos);
	else
		f->pos=pos;
	fput((task_t *)current_task, fp, 0);
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
	fput((task_t *)current_task, f, 0);
	return 0;
}

int sys_chdir(char *n, int fd)
{
	int ret;
	if(!n)
	{
		/* ok, we're comin' from a fchdir. This should be easy... */
		struct file *file = get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		/* we don't need to lock this because we own the inode - we know
		 * it wont get freed. An atomic operation will do. */
		add_atomic(&file->inode->count, 1);
		ret = ichdir(file->inode);
		fput((task_t *)current_task, fd, 0);
	} else
		ret = chdir(n);
	return ret;
}

int sys_link(char *s, char *d)
{
	if(!s || !d) 
		return -EINVAL;
	return link(s, d);
}

int sys_umask(mode_t mode)
{
	int old = current_task->cmask;
	current_task->cmask=mode;
	return old;
}

int sys_getdepth(int fd)
{
	struct file *file = get_file_pointer((task_t *)current_task, fd);
	if(!file)
		return -EBADF;
	struct inode *i = file->inode;
	int x=1;
	while(i != current_task->thread->root && i) {
		x++;
		if(i->mount_parent)
			i = i->mount_parent;
		else
			i = i->parent;
	}
	fput((task_t *)current_task, fd, 0);
	return x;
}

int sys_getcwdlen()
{
	struct inode *i = current_task->thread->pwd;
	if(!i) return 0;
	int x=64;
	while(i && i->parent)
	{
		if(i->mount_parent)
			i = i->mount_parent;
		x += strlen(i->name)+1;
		i = i->parent;
		if(i == current_task->thread->root)
			break;
		if(i->mount_parent)
			i = i->mount_parent;
		if(i == current_task->thread->root)
			break;
	}
	return x;
}

int sys_chmod(char *path, int fd, mode_t mode)
{
	if(!path && fd == -1) return -EINVAL;
	struct inode *i;
	if(path)
		i = get_idir(path, 0);
	else {
		struct file *file = get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		fput((task_t *)current_task, fd, 0);
	}
	if(!i) return -ENOENT;
	if(i->uid != current_task->thread->uid && current_task->thread->uid)
	{
		if(path)
			iput(i);
		return -EPERM;
	}
	i->mode = (i->mode&~0xFFF) | mode;
	sync_inode_tofs(i);
	if(path)
		iput(i);
	return 0;
}

int sys_chown(char *path, int fd, uid_t uid, gid_t gid)
{
	if(!path && fd == -1)
		return -EINVAL;
	struct inode *i;
	if(path)
		i = get_idir(path, 0);
	else {
		struct file *file = get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		fput((task_t *)current_task, fd, 0);
	}
	if(!i)
		return -ENOENT;
	if(current_task->thread->uid && current_task->thread->uid != i->uid) {
		if(path)
			iput(i);
		return -EPERM;
	}
	if(uid != -1) i->uid = uid;
	if(gid != -1) i->gid = gid;
	sync_inode_tofs(i);
	if(path) 
		iput(i);
	return 0;
}

int sys_utime(char *path, time_t a, time_t m)
{
	if(!path)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	if(current_task->thread->uid && current_task->thread->uid != i->uid) {
		iput(i);
		return -EPERM;
	}
	i->mtime = m ? m : (time_t)get_epoch_time();
	i->atime = a ? a : (time_t)get_epoch_time();
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

int sys_ftruncate(int f, off_t length)
{
	struct file *file = get_file_pointer((task_t *)current_task, f);
	if(!file)
		return -EBADF;
	if(!permissions(file->inode, MAY_WRITE)) {
		fput((task_t *)current_task, f, 0);
		return -EACCES;
	}
	file->inode->len = length;
	sync_inode_tofs(file->inode);
	fput((task_t *)current_task, f, 0);
	return 0;
}

int sys_mknod(char *path, mode_t mode, dev_t dev)
{
	if(!path) return -EINVAL;
	struct inode *i = lget_idir(path, 0);
	if(i) {
		iput(i);
		return -EEXIST;
	}
	i = cget_idir(path, 0, mode);
	if(!i) return -EACCES;
	i->dev = dev;
	i->mode = (mode & ~0xFFF) | ((mode&0xFFF) & (~current_task->cmask&0xFFF));
	sync_inode_tofs(i);
	if(S_ISFIFO(i->mode)) {
		i->pipe = create_pipe();
		i->pipe->type = PIPE_NAMED;
	}
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
	inode->mode &= 0x1FF;
	inode->mode |= S_IFLNK;
	inode->len=0;
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
int sys_access(char *path, mode_t mode)
{
	if(!path)
		return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i)
		return -ENOENT;
	if(current_task->thread->uid == 0) {
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
	if(!file)
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
	fput((task_t *)current_task, i, 0);
	return ready;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
		struct timeval *timeout)
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
		schedule();
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
