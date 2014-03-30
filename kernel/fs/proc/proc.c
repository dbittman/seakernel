#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/sys/stat.h>
#include <sea/dm/dev.h>
#include <sea/mm/swap.h>
#include <sea/cpu/processor.h>
#include <sea/fs/proc.h>
struct inode *procfs_root, *procfs_kprocdir;
int proc_read_int(char *buf, int off, int len);
int proc_read_mutex(char *buf, int off, int len);
int proc_read_bcache(char *buf, int off, int len);
struct inode_operations procfs_inode_ops = {
 proc_read,
 proc_write,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 0
};
int proc_mods(char rw, struct inode *n, int min, char *buf, int off, int len);
int proc_cpu(char rw, struct inode *inode, int m, char *buf, int off, int len);
int proc_vfs(char rw, struct inode *n, int m, char *buf, int off, int len);
int proc_kern_rw(char rw, struct inode *inode, int m, char *buf, int off, int len);
int proc_rw_mem(char rw, struct inode *inode, int m, char *buf, int off, int len);

int *pfs_table[64] = {
 (int *)proc_rw_mem, //Memory
 (int *)0, //Tasking
 (int *)proc_vfs, //VFS
 (int *)proc_kern_rw, //Kernel
#if CONFIG_MODULES
 (int *)proc_mods, //Modules
#else
 0,
#endif
#if CONFIG_SMP
 (int *)proc_cpu,
#else
 0,
#endif
 0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,
};

int proc_get_major()
{
	int i=10;
	for(;i<64 && pfs_table[i];++i);
	if(i == 64)
		panic(0, "failed to allocate new proc callback");
	return i;
}

int proc_set_callback(int major, int( *callback)(char rw, struct inode *inode, 
	int m, char *buf, int, int))
{
	pfs_table[major] = (int *)callback;
	return 0;
}

struct inode *proc_create_node_at_root(char *name, mode_t  mode, int major, int minor)
{
	if(!name) return 0;
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = &procfs_inode_ops;
	i->parent = procfs_root;
	i->mode = mode | 0664;
	i->dev = GETDEV(major, minor);
	rwlock_create(&i->rwl);
	vfs_add_inode(procfs_root, i);
	return i;
}

struct inode *proc_create_node(struct inode *to, char *name, mode_t mode, int major, int minor)
{
	if(!name) return 0;
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = &procfs_inode_ops;
	i->parent = procfs_root;
	i->mode = mode | 0664;
	i->dev = GETDEV(major, minor);
	rwlock_create(&i->rwl);
	vfs_add_inode(to, i);
	
	return i;
}

int proc_append_buffer(char *buffer, char *data, int off, int len, int req_off, int req_len)
{
	/* if we want to add a string, we make things easy because we may not
	 * know the length */
	if(len == -1)
		len = strlen(data);
	if(off + len <= req_off)
		return 0;
	if(off >= req_off+req_len)
		return 0;
	int data_off=0;
	if(off < req_off) {
		len = (off+len)-req_off;
		data_off = req_off - off;
		off=0;
	}
	if(off+len > req_off+req_len) {
		len = req_off+req_len - off;
	}
	memcpy(buffer+(off-req_off), data + data_off, len);
	return len;
}

void proc_init()
{
	procfs_root = (struct inode*)kmalloc(sizeof(struct inode));
	_strcpy(procfs_root->name, "proc");
	procfs_root->i_ops = &procfs_inode_ops;
	procfs_root->parent = current_task->thread->root;
	procfs_root->mode = S_IFDIR | 0774;
	procfs_root->num = -1;
	rwlock_create(&procfs_root->rwl);
	/* Create proc nodes */
	proc_create_node_at_root("mem", S_IFREG, 0, 0);
	struct inode *si = proc_create_node_at_root("sched", S_IFDIR, 1, 0);
	proc_create_node(si, "pri_tty", S_IFREG, 1, 1);
	proc_create_node_at_root("vfs", S_IFREG, 2, 0);
	proc_create_node_at_root("kernel", S_IFREG, 3, 0);
	proc_create_node_at_root("klogfile", S_IFREG, 3, 1);
	proc_create_node_at_root("version", S_IFREG, 3, 2);
	proc_create_node_at_root("swap", S_IFREG, 3, 3);
	proc_create_node_at_root("isr", S_IFREG, 3, 4);
	proc_create_node_at_root("bcache", S_IFREG, 3, 6);
	proc_create_node_at_root("modules", S_IFREG, 4, 0);
	proc_create_node_at_root("mounts", S_IFREG, 2, 2);
	proc_create_node_at_root("seaos", S_IFREG, 3, 2);
	/* Mount the filesystem */
	vfs_add_inode(current_task->thread->root, procfs_root);
}

int proc_read(struct inode *i, off_t off, size_t len, char *buffer)
{
	if(!i || !buffer) return -EINVAL;
	int maj = MAJOR(i->dev);
	if(!pfs_table[maj])
		return -ENOENT;
	int min = MINOR(i->dev);
	int (*callback)(char, struct inode *, int, char *, int off, int len);
	callback = (int(*)(char, struct inode *, int, char *, int off, int len)) pfs_table[maj];
	return callback(READ, i, min, buffer, off, len);
}

int proc_write(struct inode *i, off_t pos, size_t len, char *buffer)
{
	if(!i || !buffer) return -EINVAL;
	int maj = MAJOR(i->dev);
	if(!pfs_table[maj])
		return -ENOENT;
	int min = MINOR(i->dev);
	int (*callback)(char, struct inode *, int, char *, int, int);
	callback = (int(*)(char, struct inode *, int, char *, int, int)) pfs_table[maj];
	return callback(WRITE, i, min, buffer, pos, len);
}
