#include <sea/tm/process.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/sys/stat.h>
#include <sea/dm/char.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/loader/symbol.h>
#include <sea/fs/mount.h>
#include <sea/fs/file.h>
#include <sea/dm/block.h>
#include <sea/fs/devfs.h>
#include <sea/fs/mount.h>
#include <sea/fs/dir.h>
#include <sea/fs/proc.h>
#include <sea/fs/callback.h>
#include <sea/fs/ramfs.h>
#include <sea/dm/pipe.h>
#include <sea/tm/schedule.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/tty/terminal.h>
#include <sea/fs/socket.h>
#include <sea/mm/kmalloc.h>
static int system_setup=0;
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
		tm_schedule();
		return 1;
	}
	printk(KERN_MILE, "[kernel]: Setting up environment...");
#warning "TODO"
	struct filesystem *fs = kmalloc(sizeof(struct filesystem));
	ramfs_mount(fs);
	current_task->thread->pwd = current_task->thread->root = fs_read_root_inode(fs);
	fs_initrd_parse();
	
	kprintf("--TESTING--\n");

	char buf[16];
	int f = sys_open("/preinit.sh", O_RDWR);
	kprintf("--> %d\n", f);

	sys_read(f, 0, buf, 15);
	buf[15] = 0;
	kprintf("[%s]\n", buf);
	
	
	
	
	for(;;);
	//devfs_init();
	//proc_init();
	dm_char_rw(OPEN, GETDEV(3, 1), 0, 0);
	sys_open("/dev/tty1", O_RDWR);   /* stdin  */
	sys_open("/dev/tty1", O_WRONLY); /* stdout */
	sys_open("/dev/tty1", O_WRONLY); /* stderr */
	current_task->tty=1;
	system_setup=1;
	printk(KERN_MILE, "done (i/o/e=%x [tty1]: ok)\n", GETDEV(3, 1));
	return 12;
}
#warning "REWRITE READDIR, GETCWD"
void fs_init()
{
	fs_init_superblock_table();
	vfs_icache_init();
#warning "TODO"
#if 0
#if CONFIG_MODULES
	loader_add_kernel_symbol(vfs_do_iremove);
	loader_add_kernel_symbol(proc_create_node);
	loader_add_kernel_symbol(proc_create_node_at_root);
	loader_add_kernel_symbol(sys_open);
	loader_add_kernel_symbol(sys_read);
	loader_add_kernel_symbol(sys_write);
	loader_add_kernel_symbol(sys_close);
	loader_add_kernel_symbol(fs_inode_read);
	loader_add_kernel_symbol(fs_inode_write);
	loader_add_kernel_symbol(sys_ioctl);
	loader_add_kernel_symbol(proc_append_buffer);
	loader_add_kernel_symbol(sys_stat);
	loader_add_kernel_symbol(sys_fstat);
	loader_add_kernel_symbol(fs_register_filesystem);
	loader_add_kernel_symbol(fs_unregister_filesystem);
	loader_add_kernel_symbol(vfs_icache_put);
	loader_do_add_kernel_symbol((addr_t)(struct inode **)&devfs_root, "devfs_root");
	loader_add_kernel_symbol(vfs_do_get_idir);
	loader_add_kernel_symbol(proc_set_callback);
	loader_add_kernel_symbol(proc_get_major);
#endif
#endif
}

int sys_unlink(const char *path)
{

}

int sys_rmdir(const char *path)
{

}

int sys_mount(char *node, char *to)
{

}

int sys_mount2(char *node, char *to, char *name, char *opts, int something)
{

}

int sys_umount()
{

}

int sys_get_pwd(char *b, int s)
{

}

int sys_chroot(char *path)
{

}

int sys_seek(int fp, off_t pos, unsigned whence)
{
	struct file *f = fs_get_file_pointer((task_t *)current_task, fp);
	if(!f) return -EBADF;
	if(S_ISCHR(f->inode->mode) || S_ISFIFO(f->inode->mode)) {
		fs_fput((task_t *)current_task, fp, 0);
		return 0;
	}
	if(whence)
		f->pos = ((whence == SEEK_END) ? f->inode->length+pos : f->pos+pos);
	else
		f->pos=pos;
	fs_fput((task_t *)current_task, fp, 0);
	return f->pos;
}

int sys_fsync(int f)
{
	struct file *file = fs_get_file_pointer((task_t *)current_task, f);
	if(!file)
		return -EBADF;
	fs_inode_push(file->inode);
	fs_fput((task_t *)current_task, f, 0);
	return 0;
}

int sys_chdir(char *n, int fd)
{
	int ret;
	if(!n)
	{
		/* ok, we're comin' from a fchdir. This should be easy... */
		struct file *file = fs_get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		ret = vfs_inode_chdir(file->inode);
		fs_fput((task_t *)current_task, fd, 0);
	} else {
		struct inode *node = fs_resolve_path_inode(n, &ret);
		if(node) {
			ret = vfs_inode_chdir(node);
			vfs_icache_put(node);
		}
	}
	return ret;
}

int sys_link(char *s, char *d)
{
	if(!s || !d) 
		return -EINVAL;
#warning "todo"
	//return vfs_link(s, d);
}

int sys_umask(mode_t mode)
{
	int old = current_task->cmask;
	current_task->cmask=mode;
	return old;
}
#warning "TODO"
int sys_getdepth(int fd)
{
}
int sys_getcwdlen()
{
}

int sys_chmod(char *path, int fd, mode_t mode)
{
	if(!path && fd == -1) return -EINVAL;
	struct inode *i;
	if(path)
		i = fs_resolve_path_inode(path, 0);
	else {
		struct file *file = fs_get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		assert(i);
		fs_fput((task_t *)current_task, fd, 0);
	}
	if(!i)
		return -ENOENT;
	if(i->uid != current_task->thread->effective_uid && current_task->thread->effective_uid)
	{
		if(path)
			vfs_icache_put(i);
		else
			fs_fput((task_t *)current_task, fd, 0);
		return -EPERM;
	}
	rwlock_acquire(&i->lock, RWL_WRITER);
	i->mode = (i->mode&~0xFFF) | mode;
	i->mtime = time_get_epoch();
	vfs_inode_set_dirty(i);
	rwlock_release(&i->lock, RWL_WRITER);
	if(path)
		vfs_icache_put(i);
	else
		fs_fput((task_t *)current_task, fd, 0);
	return 0;
}

int sys_chown(char *path, int fd, uid_t uid, gid_t gid)
{
	if(!path && fd == -1)
		return -EINVAL;
	struct inode *i;
	if(path)
		i = fs_resolve_path_inode(path, 0);
	else {
		struct file *file = fs_get_file_pointer((task_t *)current_task, fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		fs_fput((task_t *)current_task, fd, 0);
	}
	if(!i)
		return -ENOENT;
	if(current_task->thread->effective_uid && current_task->thread->effective_uid != i->uid) {
		if(path)
			vfs_icache_put(i);
		return -EPERM;
	}
	if(uid != -1) i->uid = uid;
	if(gid != -1) i->gid = gid;
	if(current_task->thread->real_uid && current_task->thread->effective_uid)
	{
		/* if we're not root, we must clear these bits */
		i->mode &= ~S_ISUID;
		i->mode &= ~S_ISGID;
	}
	i->mtime = time_get_epoch();
	vfs_inode_set_dirty(i);
	if(path) 
		vfs_icache_put(i);
	return 0;
}

int sys_utime(char *path, time_t a, time_t m)
{
	if(!path)
		return -EINVAL;
	struct inode *i = fs_resolve_path_inode(path, 0);
	if(!i)
		return -ENOENT;
	if(current_task->thread->effective_uid && current_task->thread->effective_uid != i->uid) {
		vfs_icache_put(i);
		return -EPERM;
	}
	i->mtime = m ? m : (time_t)time_get_epoch();
	i->atime = a ? a : (time_t)time_get_epoch();
	vfs_inode_set_dirty(i);
	vfs_icache_put(i);
	return 0;
}

#warning "TODO"
#if 0
int sys_getnodestr(char *path, char *node)
{
	if(!path || !node)
		return -EINVAL;
	struct inode *i = fs_resolve_path_inode(path, 0);
	if(!i)
		return -ENOENT;
	strncpy(node, i->node_str, 128);
	vfs_icache_put(i);
	return 0;
}
#endif

int sys_ftruncate(int f, off_t length)
{
	struct file *file = fs_get_file_pointer((task_t *)current_task, f);
	if(!file)
		return -EBADF;
	if(!vfs_inode_check_permissions(file->inode, MAY_WRITE, 0)) {
		fs_fput((task_t *)current_task, f, 0);
		return -EACCES;
	}
	file->inode->length = length;
	file->inode->mtime = time_get_epoch();
	vfs_inode_set_dirty(file->inode);
	fs_fput((task_t *)current_task, f, 0);
	return 0;
}

int sys_mknod(char *path, mode_t mode, dev_t dev)
{
	if(!path) return -EINVAL;
	struct inode *i = fs_resolve_path_inode(path, RESOLVE_NOLINK);
	if(i) {
		vfs_icache_put(i);
		return -EEXIST;
	}
	i = fs_resolve_path_create(path, 0, mode, 0);
	if(!i) return -EACCES;
	i->phys_dev = dev;
#warning "set the dirent file type based off of mode"
	i->mode = (mode & ~0xFFF) | ((mode&0xFFF) & (~current_task->cmask&0xFFF));
	vfs_inode_set_dirty(i);
	if(S_ISFIFO(i->mode)) {
		i->pipe = dm_create_pipe();
		i->pipe->type = PIPE_NAMED;
	}
	vfs_icache_put(i);
	return 0;
}

int sys_readlink(char *_link, char *buf, int nr)
{
	if(!_link || !buf)
		return -EINVAL;
	struct inode *i = fs_resolve_path_inode(_link, RESOLVE_NOLINK);
	if(!i)
		return -ENOENT;
	int ret = fs_inode_read(i, 0, nr, buf);
	vfs_icache_put(i);
	return ret;
}

int sys_symlink(char *p2, char *p1)
{
	if(!p2 || !p1)
		return -EINVAL;
	struct inode *inode = fs_resolve_path_inode(p1, 0);
	if(!inode) inode = fs_resolve_path_inode(p1, RESOLVE_NOLINK);
	if(inode)
	{
		vfs_icache_put(inode);
		return -EEXIST;
	}
#warning "set the dirent file type based off of mode"
	inode = fs_resolve_path_create(p1, 0, 0x1FF);
	if(!inode)
		return -EACCES;
	inode->mode &= 0x1FF;
	inode->mode |= S_IFLNK;
	inode->length=0;
	int ret=0;
	vfs_inode_set_dirty(inode);
	
	if((ret=fs_inode_write(inode, 0, strlen(p2), p2)) < 0) {
		vfs_icache_put(inode);
		return ret;
	}
	vfs_icache_put(inode);
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
	struct inode *i = fs_resolve_path_inode(path, 0);
	if(!i)
		return -ENOENT;
	if(current_task->thread->real_uid == 0) {
		vfs_icache_put(i);
		return 0;
	}
	int fail=0;
	/* access uses the REAL UID to check permissions... */
	if(mode & R_OK)
		fail += (vfs_inode_check_permissions(i, MAY_READ, 1) ? 0 : 1);
	if(mode & W_OK)
		fail += (vfs_inode_check_permissions(i, MAY_WRITE, 1) ? 0 : 1);
	if(mode & X_OK)
		fail += (vfs_inode_check_permissions(i, MAY_EXEC, 1) ? 0 : 1);
	vfs_icache_put(i);
	return (fail ? -EACCES : 0);
}

static int select_filedes(int i, int rw)
{
	int ready = 1;
	struct file *file = fs_get_file_pointer((task_t *)current_task, i);
	if(!file)
		return -EBADF;
	struct inode *in = file->inode;
	if(S_ISREG(in->mode) || S_ISDIR(in->mode) || S_ISLNK(in->mode))
		ready = fs_callback_inode_select(in, rw);
	else if(S_ISCHR(in->mode))
		ready = dm_chardev_select(in, rw);
	else if(S_ISBLK(in->mode))
		ready = dm_blockdev_select(in, rw);
	else if(S_ISFIFO(in->mode))
		ready = dm_pipedev_select(in, rw);
	else if(S_ISSOCK(in->mode))
		ready = socket_select(file, rw);
	fs_fput((task_t *)current_task, i, 0);
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
	long end = wait+tm_get_ticks();
	int total_set=0, is_ok=0;
	int ret=0;
	while((tm_get_ticks() <= end || !wait || !timeout) && !ret)
	{
		total_set=0;
		for(i=0;i<nfds;++i)
		{
			if(readfds && FD_ISSET(i, readfds))
			{
				if(select_filedes(i, READ)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, readfds);
			}
			if(writefds && FD_ISSET(i, writefds))
			{
				if(select_filedes(i, WRITE)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, writefds);
			}
			if(errorfds && FD_ISSET(i, errorfds))
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
		tm_schedule();
		if(tm_process_got_signal(current_task))
			return -EINTR;
	}
	if(timeout)
	{
		timeout->tv_sec = (end-tm_get_ticks())/1000;
		timeout->tv_usec = ((end-tm_get_ticks())%1000)*1000;
		if(tm_get_ticks() >= end) {
			timeout->tv_sec = 0;
			timeout->tv_usec = 0;
			return 0;
		}
	}
	return ret;
}
