#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mm/vmm.h>
#include <sea/dm/dev.h>
#include <sea/dm/char.h>
#include <sea/dm/block.h>
#include <sea/fs/inode.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/devfs.h>

struct inode *devfs_root;
static int devfs_nodescount=1;

struct inode_operations devfs_inode_ops = {
 0,
 0,
 0,
 devfs_create,
 0,
 0,
 0,
 0,
 0,
 0,
 0,
 devfs_fsstat,
 0,
 0
};

int devfs_fsstat(struct inode *i, struct posix_statfs *fs)
{
	memset(fs, 0, sizeof(*fs));
	fs->f_type = 0x1373;
	fs->f_fsid = 4;
	return 0;
}

void devfs_init()
{
	devfs_root = (struct inode*)kmalloc(sizeof(struct inode));
	_strcpy(devfs_root->name, "dev");
	devfs_root->i_ops = &devfs_inode_ops;
	devfs_root->parent = current_task->thread->root;
	devfs_root->mode = S_IFDIR | 0774;
	devfs_root->num = -1;
	mutex_create(&devfs_root->mappings_lock, 0);
	rwlock_create(&devfs_root->rwl);
	/* Create device nodes */
	char tty[6] = "tty";
	int i;
	for(i=1;i<10;i++) {
		snprintf(tty, 6, "tty%d", i);
		devfs_add(devfs_root, tty, S_IFCHR, 3, i);
	}
	devfs_add(devfs_root, "tty", S_IFCHR, 4, 0);
	devfs_add(devfs_root, "null", S_IFCHR, 0, 0);
	devfs_add(devfs_root, "zero", S_IFCHR, 1, 0);
	devfs_add(devfs_root, "com0", S_IFCHR, 5, 0);
	/* Mount the filesystem */
	vfs_add_inode(current_task->thread->root, devfs_root);
#if CONFIG_MODULES
	loader_add_kernel_symbol(devfs_add);
	loader_add_kernel_symbol(devfs_remove);
#endif
}

struct inode *devfs_add(struct inode *q, char *name, mode_t mode, int major, int minor)
{
	struct inode *i = devfs_create(q, name, mode);
	i->dev = GETDEV(major, minor);
	return i;
}

struct inode *devfs_create(struct inode *base, char *name, mode_t mode)
{
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = &devfs_inode_ops;
	i->parent = devfs_root;
	i->mode = mode | 0664;
	i->num = add_atomic(&devfs_nodescount, 1);
	rwlock_create(&i->rwl);
	mutex_create(&i->mappings_lock, 0);
	vfs_add_inode(base, i);
	return i;
}

void devfs_remove(struct inode *i)
{
	rwlock_acquire(&i->rwl, RWL_WRITER);
	i->count=0;
	iremove(i);
}
