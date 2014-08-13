#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/ramfs.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>
struct inode *ramfs_root;

static unsigned int ramfs_node_num=0;

int ramfs_op_dummy()
{
	return 0;
}

int ramfs_unlink(struct inode *i)
{
	return 0;
}

static struct inode_operations rfs_inode_ops = {
 fs_ramfs_read,
 fs_ramfs_write,
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

struct inode *fs_init_ramfs()
{
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	i->mode = S_IFDIR | 0664;
	rwlock_create(&i->rwl);
	mutex_create(&i->mappings_lock, 0);
	_strcpy(i->name, "rfs");
	ramfs_root = i;
	i->i_ops = &rfs_inode_ops;
	return i;
}

struct inode *fs_init_tmpfs()
{
	struct inode *i = (struct inode *)kmalloc(sizeof(struct inode));
	i->mode = S_IFDIR | 0x664;
	rwlock_create(&i->rwl);
	mutex_create(&i->mappings_lock, 0);
	_strcpy(i->name, "rfs");
	i->i_ops = &rfs_inode_ops;
	return i;
}

int fs_ramfs_read(struct inode *i, off_t off, size_t len, char *b)
{
	size_t pl = len;
	if(off >= i->len)
		return 0;
	if((off+len) >= (unsigned)i->len)
		len = i->len-off;
	if(!len)
		return 0;
	memcpy((void *)b, (void *)(i->start+(addr_t)off), len);
	return len;
}

static void rfs_resize(struct inode *i, off_t s)
{
	if(s == i->len)
		return;
	addr_t new = (addr_t)kmalloc(s);
	if(i->len > s)
		memcpy((void *)new, (void *)i->start, s);
	else
		memcpy((void *)new, (void *)i->start, i->len);
	void *old = (void *)i->start;
	i->start = new;
	i->len = s;
	kfree(old);
}

int fs_ramfs_write(struct inode *i, off_t off, size_t len, char *b)
{
	if(!len)
		return -EINVAL;
	if(off > i->len || off+len > (unsigned)i->len) 
	{
		rwlock_acquire(&i->rwl, RWL_WRITER);
		rfs_resize(i, len+off);
		rwlock_release(&i->rwl, RWL_WRITER);
	}
	memcpy((void *)(i->start+(addr_t)off), (void *)b, len);
	return len;
}

struct inode *rfs_create(struct inode *__p, char *name, mode_t mode)
{
	struct inode *r, *p=__p;
	if(!__p)
		p = ramfs_root;
	struct inode *node;
	node = (struct inode *)kmalloc(sizeof(struct inode));
	strncpy(node->name, name, INAME_LEN);
	node->uid = current_task->thread->effective_uid;
	node->gid = current_task->thread->effective_gid;
	node->len = 0;
	node->i_ops = &rfs_inode_ops;
	node->mode = mode | 0664;
	node->start = (addr_t)kmalloc(1);
	node->num = add_atomic(&ramfs_node_num, 1);
	rwlock_create(&node->rwl);
	mutex_create(&node->mappings_lock, 0);
	if(!__p) vfs_add_inode(p, node);
	return node;
}
