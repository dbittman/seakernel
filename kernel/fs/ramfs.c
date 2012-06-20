#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
struct inode *ramfs_root;
int ramfs_sane(struct inode *i);
unsigned int ramfs_node_num=1;
int ramfs_op_dummy()
{
	return 0;
}

int ramfs_unlink(struct inode *i)
{
	i->f_count=0;
	i->count=1;
	return 0;
}

struct inode_operations rfs_inode_ops = {
 rfs_read,
 rfs_write,
 (void *)ramfs_op_dummy,
 rfs_create,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_unlink,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
 (void *)ramfs_op_dummy,
};

struct inode *init_ramfs()
{
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	i->mode = S_IFDIR | 0x1FF;
	create_mutex(&i->lock);
	_strcpy(i->name, "rfs");
	ramfs_root = i;
	i->i_ops = &rfs_inode_ops;
	i->parent = i;
	return i;
}

struct inode *init_tmpfs()
{
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	i->mode = S_IFDIR | 0x1FF;
	create_mutex(&i->lock);
	_strcpy(i->name, "rfs");
	i->i_ops = &rfs_inode_ops;
	i->parent = i;
	return i;
}

int rfs_read(struct inode *i, unsigned int off, unsigned int len, char *b)
{
	int pl = len;
	if((unsigned)off >= i->len)
		return 0;
	if((unsigned)(off+len) >= i->len)
		len = i->len-off;
	if(!len)
		return 0;
	memcpy((void *)b, (void *)(i->start+off), len);
	return len;
}

static void rfs_resize(struct inode *i, unsigned int s)
{
	if(s == i->len)
		return;
	int new = (int)kmalloc(s);
	if(i->len > s)
	{
		memcpy((void *)new, (void *)i->start, s);
	}
	else
	{
		memcpy((void *)new, (void *)i->start, i->len);
	}
	kfree((void *)i->start);
	i->start = new;
	i->len = s;
}

int rfs_write(struct inode *i, unsigned int off, unsigned int len, char *b)
{
	if(!len)
		return -EINVAL;
	if((unsigned)off > i->len || (unsigned)off+len > i->len) 
	{
		mutex_on(&i->lock);
		rfs_resize(i, len+off);
		mutex_off(&i->lock);
	}
	memcpy((void *)(i->start+off), (void *)b, len);
	return len;
}

struct inode *rfs_create(struct inode *__p, char *name, unsigned int mode)
{
	struct inode *r, *p=__p;
	if(!__p)
		p = ramfs_root;
	if((r = (struct inode *)get_idir(name, p)))
	{
		return r;
	}
	struct inode *node;
	node = (struct inode *)kmalloc(sizeof(struct inode));
	strncpy(node->name, name, INAME_LEN);
	node->uid = current_task->uid;
	node->gid = current_task->gid;
	node->len = 0;
	node->i_ops = &rfs_inode_ops;
	node->mode = mode | 0x1FF;
	node->start = (int)kmalloc(1);
	task_critical();
	node->num = ramfs_node_num++;
	task_uncritical();
	create_mutex(&node->lock);
	if(!__p) add_inode(p, node);
	return node;
}
