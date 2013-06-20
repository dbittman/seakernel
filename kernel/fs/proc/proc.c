#include <types.h>
#include <kernel.h>
#include <fs.h>
#include <task.h>
#include <sys/stat.h>
#include <dev.h>
#include <mod.h>
#include <swap.h>
#include <cpu.h>
struct inode *procfs_root, *procfs_kprocdir;
int proc_read_int(char *buf, int off, int len);
int proc_read_mutex(char *buf, int off, int len);
int proc_read_bcache(char *buf, int off, int len);
extern struct inode_operations procfs_inode_ops;
int proc_mods(char rw, struct inode *n, int min, char *buf, int off, int len);
int proc_cpu(char rw, struct inode *inode, int m, char *buf, int off, int len);
int proc_vfs(char rw, struct inode *n, int m, char *buf, int off, int len);
int proc_kern_rw(char rw, struct inode *inode, int m, char *buf, int off, int len);

int *pfs_table[64] = {
 0, //Memory
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

struct inode *pfs_cn(char *name, mode_t  mode, int major, int minor)
{
	if(!name) return 0;
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = &procfs_inode_ops;
	i->parent = procfs_root;
	i->mode = mode | 0xFFF;
	i->dev = 256*major+minor;
	rwlock_create(&i->rwl);
	add_inode(procfs_root, i);
	return i;
}

struct inode *pfs_cn_node(struct inode *to, char *name, mode_t mode, int major, int minor)
{
	if(!name) return 0;
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = &procfs_inode_ops;
	i->parent = procfs_root;
	i->mode = mode | 0x1FF;
	i->dev = 256*major+minor;
	rwlock_create(&i->rwl);
	add_inode(to, i);
	
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

void init_proc_fs()
{
	procfs_root = (struct inode*)kmalloc(sizeof(struct inode));
	_strcpy(procfs_root->name, "proc");
	procfs_root->i_ops = &procfs_inode_ops;
	procfs_root->parent = current_task->thread->root;
	procfs_root->mode = S_IFDIR | 0x1FF;
	procfs_root->num = -1;
	rwlock_create(&procfs_root->rwl);
	/* Create proc nodes */
	pfs_cn("mem", S_IFREG, 0, 0);
	struct inode *si = pfs_cn("sched", S_IFDIR, 1, 0);
	pfs_cn_node(si, "pri_tty", S_IFREG, 1, 1);
	pfs_cn("vfs", S_IFREG, 2, 0);
	pfs_cn("kernel", S_IFREG, 3, 0);
	pfs_cn("klogfile", S_IFREG, 3, 1);
	pfs_cn("version", S_IFREG, 3, 2);
	pfs_cn("swap", S_IFREG, 3, 3);
	pfs_cn("isr", S_IFREG, 3, 4);
	pfs_cn("bcache", S_IFREG, 3, 6);
	pfs_cn("modules", S_IFREG, 4, 0);
	pfs_cn("mounts", S_IFREG, 2, 1);
	pfs_cn("seaos", S_IFREG, 3, 2);
	/* Mount the filesystem */
	add_inode(current_task->thread->root, procfs_root);
}

int pfs_read(struct inode *i, off_t off, size_t len, char *buffer)
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

int pfs_write(struct inode *i, off_t pos, size_t len, char *buffer)
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

struct inode_operations procfs_inode_ops = {
 pfs_read,
 pfs_write,
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
