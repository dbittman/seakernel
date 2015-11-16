
#include <sea/dm/block.h>
#include <sea/dm/char.h>
#include <sea/dm/dev.h>
#include <sea/errno.h>
#include <sea/fs/callback.h>
#include <sea/fs/devfs.h>
#include <sea/fs/dir.h>
#include <sea/fs/file.h>
#include <sea/fs/fs.h>
#include <sea/fs/inode.h>
#include <sea/fs/kerfs.h>
#include <sea/fs/mount.h>
#include <sea/fs/pipe.h>
#include <sea/fs/proc.h>
#include <sea/fs/ramfs.h>
#include <sea/fs/socket.h>
#include <sea/kernel.h>
#include <sea/loader/symbol.h>
#include <sea/mm/kmalloc.h>
#include <sea/rwlock.h>
#include <sea/string.h>
#include <sea/sys/fcntl.h>
#include <sea/sys/stat.h>
#include <sea/tm/process.h>
#include <sea/tm/timing.h>
#include <sea/tty/terminal.h>
#include <sea/vsprintf.h>
#include <sea/trace.h>
#include <sea/syslog.h>
static int system_setup=0;
/* This function is called once at the start of the init process initialization.
 * It sets the task fs values to possible and useful things, allowing VFS access.
 * It then starts the device and proc filesystems, and opens up /dev/tty1 on
 * file desciptors 0, 1 and 2 (std[in,out,err]).
 * 
 * Beyond that, it can be called by any task at anytime after the first call as
 * a yield call.
 */
extern void fs_initrd_parse();

extern struct filesystem *devfs;

void devfs_init(void)
{
	fs_filesystem_init_mount(devfs, "/dev", "devfs", "devfs", 0);
	ramfs_mount(devfs);
	int r;
	struct inode *in = fs_path_resolve_inode("/dev", 0, &r);
	if(!in) {
		panic(0, "/dev does not exist (%d)", -r);
	}
	if(!S_ISDIR(in->mode)) {
		panic(0, "/dev is not a directory");
	}
	r = fs_mount(in, devfs);
	vfs_icache_put(in);
	char tty[16];
	for(int i=1;i<10;i++) {
		snprintf(tty, 10, "/dev/tty%d", i);
		sys_mknod(tty, S_IFCHR | 0600, GETDEV(3, i));
	}
	sys_mknod("/dev/tty", S_IFCHR | 0600, GETDEV(4, 0));
	sys_mknod("/dev/null", S_IFCHR | 0666, GETDEV(0, 0));
	sys_mknod("/dev/zero", S_IFCHR | 0666, GETDEV(1, 0));
	sys_mknod("/dev/com0", S_IFCHR | 0600, GETDEV(5, 0));
	sys_mkdir("/dev/process", 0755);
}

int kerfs_trace_on(int direction, void *_, size_t __, size_t offset, size_t length, char *buf)
{
	if(direction != WRITE)
		return -EIO;
	if(offset > 0)
		return 0;
	if(length > 128)
		length = 128;
	char tmp[length + 1];
	memset(tmp, 0, length + 1);
	strncpy(tmp, buf, length);
	char *n;
	if((n = strrchr(tmp, '\n')))
		*n = 0;
	trace_on(tmp);
	return length;
}

int kerfs_pfault_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf);

int kerfs_syslog(int direction, void *param, size_t size, size_t offset, size_t length, char *buf);
int kerfs_trace_off(int direction, void *_, size_t __, size_t offset, size_t length, char *buf)
{
	if(direction != WRITE)
		return -EIO;
	if(offset > 0)
		return 0;
	if(length > 128)
		length = 128;
	char tmp[length + 1];
	memset(tmp, 0, length + 1);
	strncpy(tmp, buf, length);
	char *n;
	if((n = strrchr(tmp, '\n')))
		*n = 0;
	trace_off(tmp);
	return length;
}

int sys_setup(int a)
{
	if(system_setup)
	{
		tm_schedule();
		return 1;
	}
	printk(KERN_MILE, "[kernel]: Setting up environment...");
	kerfs_init();

	struct filesystem *fs = fs_filesystem_create();
	ramfs_mount(fs);
	/* TODO: general read only FS stuff */
	current_process->root = fs_read_root_inode(fs);
	if(!current_process->root)
		panic(0, "failed to read root filesystem");
	vfs_inode_get(current_process->root);
	current_process->cwd = current_process->root;
	fs_initrd_parse();
	devfs_init();

	//dm_char_rw(OPEN, GETDEV(3, 1), 0, 0);
	sys_open("/dev/tty1", O_RDWR);   /* stdin  */
	sys_open("/dev/tty1", O_WRONLY); /* stdout */
	sys_open("/dev/tty1", O_WRONLY); /* stderr */
	kerfs_register_report("/dev/syscall", kerfs_syscall_report);
	kerfs_register_report("/dev/int", kerfs_int_report);
	kerfs_register_report("/dev/mounts", kerfs_mount_report);
	kerfs_register_report("/dev/pmm", kerfs_pmm_report);
	kerfs_register_report("/dev/kmm", kerfs_kmalloc_report);
	kerfs_register_report("/dev/route", kerfs_route_report);
	kerfs_register_report("/dev/fs_icache", kerfs_icache_report);
	kerfs_register_report("/dev/modules", kerfs_module_report);
	kerfs_register_report("/dev/pfault", kerfs_pfault_report);
	kerfs_register_report("/dev/syslog", kerfs_syslog);
	kerfs_register_parameter("/dev/trace_on", NULL, 0, KERFS_PARAM_WRITE, kerfs_trace_on);
	kerfs_register_parameter("/dev/trace_off", NULL, 0, KERFS_PARAM_WRITE, kerfs_trace_off);
	current_process->tty=1; /* TODO */
	tm_process_create_kerfs_entries(current_process);
	tm_thread_create_kerfs_entries(current_thread);
	system_setup=1;
	printk(KERN_MILE, "done (i/o/e=%x [tty1]: ok)\n", GETDEV(3, 1));
	return 12;
}

void fs_init(void)
{
	vfs_icache_init();
	vfs_dirent_init();
	fs_fsm_init();
#if CONFIG_MODULES
	loader_add_kernel_symbol(sys_open);
	loader_add_kernel_symbol(sys_read);
	loader_add_kernel_symbol(sys_write);
	loader_add_kernel_symbol(sys_close);
	loader_add_kernel_symbol(fs_inode_read);
	loader_add_kernel_symbol(fs_inode_write);
	loader_add_kernel_symbol(sys_ioctl);
	loader_add_kernel_symbol(sys_stat);
	loader_add_kernel_symbol(sys_fstat);
	loader_add_kernel_symbol(vfs_icache_put);
	loader_add_kernel_symbol(fs_path_resolve_inode);
	loader_add_kernel_symbol(sys_mknod);
	loader_add_kernel_symbol(sys_unlink);
	loader_add_kernel_symbol(vfs_inode_set_needread);
	loader_add_kernel_symbol(fs_filesystem_register);
	loader_add_kernel_symbol(fs_filesystem_unregister);
	loader_add_kernel_symbol(vfs_inode_set_dirty);
	loader_add_kernel_symbol(vfs_icache_get);
	loader_add_kernel_symbol(vfs_icache_put);
#endif
}

static int do_sys_unlink(const char *path, int allow_dir)
{
	int len = strlen(path) + 1;
	char tmp[len];
	memcpy(tmp, path, len);
	char *del = strrchr(tmp, '/');
	if(del) *del = 0;
	char *dir = del ? tmp : ".";
	char *name = del ? del + 1 : tmp;
	if(dir[0] == 0)
		dir = "/";

	int result;
	struct inode *node = fs_path_resolve_inode(dir, 0, &result);
	if(!node)
		return result;
	if(!vfs_inode_check_permissions(node, MAY_WRITE, 0)) {
		vfs_icache_put(node);
		return -EACCES;
	}
	int r = fs_unlink(node, name, strlen(name));
	vfs_icache_put(node);
	return r;
}

int sys_unlink(const char *path)
{
	return do_sys_unlink(path, 0);
}

int sys_mkdir(const char *path, mode_t mode)
{
	int c;
	struct inode *n;
	if(!(n=fs_path_resolve_create(path, 0, S_IFDIR | mode, &c))) {
		return c;
	}

	vfs_icache_put(n);
	if(!c)
		return -EEXIST;
	return 0;
}

int sys_rmdir(const char *path)
{
	return do_sys_unlink(path, 1);
}

int sys_fs_mount(char *node, char *point, char *type, int opts)
{
	if(current_process->effective_uid)
		return -EPERM;
	struct filesystem *fs = fs_filesystem_create();
	int r = fs_filesystem_init_mount(fs, point, node, type, opts);
	if(r) {
		fs_filesystem_destroy(fs);
		return r;
	}
	struct inode *in = fs_path_resolve_inode(point, 0, &r);
	if(!in) {
		fs_filesystem_destroy(fs);
		return r;
	}
	if(!S_ISDIR(in->mode)) {
		fs_filesystem_destroy(fs);
		return -ENOTDIR;
	}
	r = fs_mount(in, fs);
	vfs_icache_put(in);
	if(r) {
		fs_filesystem_destroy(fs);
		return r;
	}
	return 0;
}

int sys_mount(char *node, char *to)
{
	return sys_fs_mount(node, to, 0, 0);
}

int sys_mount2(char *node, char *to, char *name, char *opts, int something)
{
	return sys_fs_mount(node, to, name, 0);
}

int sys_umount(char *dir, int flags)
{
	if(current_process->effective_uid)
		return -EPERM;
	int result;
	struct inode *node = fs_path_resolve_inode(dir, RESOLVE_NOMOUNT, &result);
	if(!node)
		return result;
	if(!node->mount)
		return -EINVAL;
	if(node->mount->usecount > 0) {
		return -EBUSY;
	}
	int r = fs_umount(node->mount);
	vfs_icache_put(node);
	return r;
}

int sys_chroot(char *path)
{
	if(current_process->effective_uid)
		return -EPERM;
	int r = sys_chdir(path, 0);
	if(r)
		return r;
	struct inode *node = fs_path_resolve_inode(path, 0, &r);
	if(!node)
		return r;
	vfs_inode_chroot(node);
	vfs_icache_put(node);
	return 0;
}

int sys_seek(int fp, off_t pos, unsigned whence)
{
	struct file *f = file_get(fp);
	if(!f) return -EBADF;
	if(S_ISCHR(f->inode->mode) || S_ISFIFO(f->inode->mode)) {
		file_put(f);
		return 0;
	}
	if(whence)
		f->pos = ((whence == SEEK_END) ? f->inode->length+pos : (size_t)f->pos+pos);
	else
		f->pos=pos;
	file_put(f);
	return f->pos;
}

int sys_fsync(int f)
{
	struct file *file = file_get(f);
	if(!file)
		return -EBADF;
	fs_inode_push(file->inode);
	file_put(file);
	return 0;
}

int sys_chdir(char *n, int fd)
{
	int ret;
	if(!n)
	{
		/* ok, we're comin' from a fchdir. This should be easy... */
		struct file *file = file_get(fd);
		if(!file)
			return -EBADF;
		if(!vfs_inode_check_permissions(file->inode, MAY_EXEC, 0)) {
			file_put(file);
			return -EACCES;
		}
		ret = vfs_inode_chdir(file->inode);
		file_put(file);
	} else {
		struct inode *node = fs_path_resolve_inode(n, 0, &ret);
		if(node) {
			if(!vfs_inode_check_permissions(node, MAY_EXEC, 0)) {
				vfs_icache_put(node);
				return -EACCES;
			}
			ret = vfs_inode_chdir(node);
			vfs_icache_put(node);
		}
	}
	return ret;
}

int sys_link(char *oldpath, char *newpath)
{
	int result;
	struct inode *target = fs_path_resolve_inode(oldpath, 0, &result);
	if(!target)
		return result;

	int len = strlen(newpath) + 1;
	char tmp[len];
	memcpy(tmp, newpath, len);
	char *del = strrchr(tmp, '/');
	if(del) *del = 0;
	char *dir = del ? tmp : ".";
	char *name = del ? del + 1 : tmp;
	if(dir[0] == 0)
		dir = "/";

	struct inode *parent = fs_path_resolve_inode(dir, 0, &result);
	if(!parent) {
		vfs_icache_put(target);
		return result;
	}
	if(!vfs_inode_check_permissions(parent, MAY_WRITE, 0)) {
		vfs_icache_put(parent);
		vfs_icache_put(target);
		return -EACCES;
	}
	struct dirent *exist = fs_path_resolve(newpath, 0, &result);
	if(exist) {
		vfs_dirent_release(exist);
		fs_unlink(parent, name, strlen(name));
	}
	int r = -ENOSPC;
	if(parent->nlink > 1)
		r = fs_link(parent, target, name, strlen(name), false);
	vfs_icache_put(parent);
	vfs_icache_put(target);
	return r;
}

int sys_umask(mode_t mode)
{
	int old = current_process->cmask;
	current_process->cmask=mode;
	return old;
}

int sys_chmod(char *path, int fd, mode_t mode)
{
	if(!path && fd == -1) return -EINVAL;
	struct inode *i;
	int result;
	struct file *file = NULL;
	if(path)
		i = fs_path_resolve_inode(path, 0, &result);
	else {
		file = file_get(fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		assert(i);
	}
	if(!i)
		return result;
	if(i->uid != current_process->effective_uid && current_process->effective_uid)
	{
		if(path)
			vfs_icache_put(i);
		else
			file_put(file);
		return -EPERM;
	}
	//rwlock_acquire(&i->lock, RWL_WRITER);
	i->mode = (i->mode&~0xFFF) | mode;
	i->mtime = time_get_epoch();
	vfs_inode_set_dirty(i);
	//rwlock_release(&i->lock, RWL_WRITER);
	if(path)
		vfs_icache_put(i);
	else if(file)
		file_put(file);
	return 0;
}

int sys_chown(char *path, int fd, uid_t uid, gid_t gid)
{
	if(!path && fd == -1)
		return -EINVAL;
	struct inode *i;
	int result;
	if(path)
		i = fs_path_resolve_inode(path, 0, &result);
	else {
		struct file *file = file_get(fd);
		if(!file)
			return -EBADF;
		i = file->inode;
		/* TODO we can't do this yet! Wait until the end of the function! */
		file_put(file);
	}
	if(!i)
		return result;
	if(current_process->effective_uid && current_process->effective_uid != i->uid) {
		if(path)
			vfs_icache_put(i);
		return -EPERM;
	}
	if(uid != -1) i->uid = uid;
	if(gid != -1) i->gid = gid;
	if(current_process->real_uid && current_process->effective_uid)
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
	int result;
	struct inode *i = fs_path_resolve_inode(path, 0, &result);
	if(!i)
		return result;
	if(current_process->effective_uid && current_process->effective_uid != i->uid) {
		vfs_icache_put(i);
		return -EPERM;
	}
	i->mtime = m ? m : (time_t)time_get_epoch();
	i->atime = a ? a : (time_t)time_get_epoch();
	vfs_inode_set_dirty(i);
	vfs_icache_put(i);
	return 0;
}

int sys_ftruncate(int f, off_t length)
{
	struct file *file = file_get(f);
	if(!file)
		return -EBADF;
	if(!vfs_inode_check_permissions(file->inode, MAY_WRITE, 0)) {
		file_put(file);
		return -EACCES;
	}
	file->inode->length = length;
	file->inode->mtime = time_get_epoch();
	vfs_inode_set_dirty(file->inode);
	file_put(file);
	return 0;
}

int sys_mknod(char *path, mode_t mode, dev_t dev)
{
	if(!path) return -EINVAL;
	int result;
	struct inode *i = fs_path_resolve_inode(path, RESOLVE_NOLINK, &result);
	if(!i && result != -ENOENT)
		return result;
	if(i) {
		vfs_icache_put(i);
		return -EEXIST;
	}
	int created;
	i = fs_path_resolve_create(path, 0, mode, &created);
	if(!i) return created;
	if(!created) {
		vfs_icache_put(i);
		return -EEXIST;
	}
	i->phys_dev = dev;
	i->mode = (mode & ~0xFFF) | ((mode&0xFFF) & (~current_process->cmask&0xFFF));
	vfs_inode_set_dirty(i);
	vfs_icache_put(i);
	return 0;
}

int sys_readlink(char *_link, char *buf, int nr)
{
	if(!_link || !buf)
		return -EINVAL;
	int res;
	struct inode *i = fs_path_resolve_inode(_link, RESOLVE_NOLINK, &res);
	if(!i)
		return res;
	if(!S_ISLNK(i->mode)) {
		vfs_icache_put(i);
		return -EINVAL;
	}
	int ret = fs_inode_read(i, 0, nr, buf);
	vfs_icache_put(i);
	return ret;
}

int sys_symlink(char *p2, char *p1)
{
	if(!p2 || !p1)
		return -EINVAL;
	int res;
	struct inode *inode = fs_path_resolve_inode(p1, 0, &res);
	if(!inode) {
		if(res != -ENOENT)
			return res;
		inode = fs_path_resolve_inode(p1, RESOLVE_NOLINK, &res);
		if(!inode && res != -ENOENT)
			return res;
	}
	if(inode)
	{
		vfs_icache_put(inode);
		return -EEXIST;
	}
	inode = fs_path_resolve_create(p1, 0, S_IFLNK | 0755, &res);
	if(!inode)
		return res;
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
	int res;
	struct inode *i = fs_path_resolve_inode(path, 0, &res);
	if(!i)
		return res;
	if(current_process->real_uid == 0) {
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
	struct file *file = file_get(i);
	if(!file)
		return -EBADF;
	struct inode *in = file->inode;
	if(in->kdev.select)
		ready = in->kdev.select(file, rw);
	if(in->pty)
		ready = pty_select(in, rw);
	else if(S_ISREG(in->mode) || S_ISDIR(in->mode) || S_ISLNK(in->mode))
		ready = fs_callback_inode_select(in, rw);
	else if(S_ISCHR(in->mode))
		ready = dm_chardev_select(file, rw);
	else if(S_ISBLK(in->mode))
		ready = dm_blockdev_select(in, rw);
	else if(S_ISFIFO(in->mode))
		ready = fs_pipe_select(in, rw);
	else if(S_ISSOCK(in->mode))
		ready = socket_select(file, rw);
	file_put(file);
	return ready;
}

int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, 
		struct timeval *timeout)
{
	if(nfds < 0)
		return -EINVAL;
	unsigned long wait=0;
	int i;
	if(timeout)
		wait = timeout->tv_sec * ONE_SECOND + (timeout->tv_usec);
	time_t end = wait+tm_timing_get_microseconds();
	int total_set=0, is_ok=0;
	int ret=0;
	fd_set ret_read;
	fd_set ret_write;
	fd_set ret_err;
	FD_ZERO(&ret_read);
	FD_ZERO(&ret_write);
	FD_ZERO(&ret_err);
	while((tm_timing_get_microseconds() <= end || !wait || !timeout) && !ret)
	{
		total_set=0;
		for(i=0;i<nfds;++i)
		{
			if(readfds && FD_ISSET(i, readfds))
			{
				FD_SET(i, &ret_read);
				if(select_filedes(i, READ)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, &ret_read);
			}
			if(writefds && FD_ISSET(i, writefds))
			{
				FD_SET(i, &ret_write);
				if(select_filedes(i, WRITE)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, &ret_write);
			}
			if(errorfds && FD_ISSET(i, errorfds))
			{
				FD_SET(i, &ret_err);
				if(select_filedes(i, OTHER)) {
					++is_ok;
					++total_set;
				} else
					FD_CLR(i, &ret_err);
			}
		}
		if(!ret)
			ret = total_set;
		if((!wait && timeout) || is_ok)
			break;
		tm_schedule();
		if(tm_thread_got_signal(current_thread))
			return -EINTR;
	}
	if(readfds)
		memcpy(readfds, &ret_read, sizeof(ret_read));
	if(writefds)
		memcpy(writefds, &ret_write, sizeof(ret_write));
	if(errorfds)
		memcpy(errorfds, &ret_err, sizeof(ret_err));
	if(timeout)
	{
		timeout->tv_sec = (end-tm_timing_get_microseconds())/ONE_SECOND;
		timeout->tv_usec = ((end-tm_timing_get_microseconds())%ONE_SECOND);
		if(tm_timing_get_microseconds() >= end) {
			timeout->tv_sec = 0;
			timeout->tv_usec = 0;
			return 0;
		}
	}
	return ret;
}

