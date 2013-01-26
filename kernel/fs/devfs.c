#include <kernel.h>
#include <task.h>
#include <memory.h>
#include <dev.h>
#include <char.h>
#include <block.h>
#include <fs.h>
struct inode *devfs_root;
struct inode *dfs_cn(char *name, mode_t mode, int major, int minor);
int devfs_nodescount=1;

void init_dev_fs()
{
	devfs_root = (struct inode*)kmalloc(sizeof(struct inode));
	_strcpy(devfs_root->name, "dev");
	devfs_root->i_ops = 0;
	devfs_root->parent = current_task->root;
	devfs_root->mode = S_IFDIR | 0x1FF;
	devfs_root->uid = devfs_root->gid = GOD;
	devfs_root->num = -1;
	rwlock_create(&devfs_root->rwl);
	/* Create device nodes */
	char tty[6] = "tty";
	int i;
	for(i=1;i<10;i++) {
		sprintf(tty, "tty%d", i);
		dfs_cn(tty, S_IFCHR, 3, i);
	}
	dfs_cn("tty", S_IFCHR, 4, 0);
	dfs_cn("null", S_IFCHR, 0, 0);
	dfs_cn("zero", S_IFCHR, 1, 0);
	dfs_cn("com0", S_IFCHR, 5, 0);
	/* Mount the filesystem */
	add_inode(current_task->root, devfs_root);
}

struct inode *dfs_add(struct inode *q, char *name, mode_t mode, int major, int minor)
{
	struct inode *i;
	i = (struct inode*)kmalloc(sizeof(struct inode));
	strncpy(i->name, name, INAME_LEN);
	i->i_ops = 0;
	i->parent = devfs_root;
	i->mode = mode | 0x1FF;
	i->uid = GOD;
	i->dev = 256*major+minor;
	i->num = devfs_nodescount++;
	rwlock_create(&i->rwl);
	add_inode(q, i);
	return i;
}

struct inode *dfs_cn(char *name, mode_t mode, int major, int minor)
{
	if(!name) return 0;
	return dfs_add(devfs_root, name, mode, major, minor);
}

void remove_dfs_node(char *name)
{
	if(!name) return;
	struct inode *r = lookup(devfs_root, name);
	iremove_force(r);
}
