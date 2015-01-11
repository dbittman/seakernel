#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/fs/fs.h>
#include <sea/lib/hash.h>
#include <sea/mm/kmalloc.h>

#include <sea/cpu/atomic.h>
#include <sea/ll.h>
#include <sea/errno.h>

struct filesystem_inode_callbacks ramfs_iops;
struct filesystem_callbacks ramfs_fsops;

struct rfsdirent {
	char name[DNAME_LEN];
	size_t namelen;
	uint32_t ino;
	struct llistnode *lnode;
};

struct rfsnode {
	void *data;
	size_t length;
	uint32_t num;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	uint16_t nlinks;
	struct llist *ents;
};

struct rfsinfo {
	struct hash_table *nodes;
	uint32_t next_id;
};

int ramfs_mount(struct filesystem *fs)
{
	struct hash_table *h = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(h, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(h, HASH_FUNCTION_BYTE_SUM);
	struct rfsnode *root = kmalloc(sizeof(struct rfsnode));
	root->mode = S_IFDIR | 0755;
	root->ents = ll_create(0);

	struct rfsinfo *info = kmalloc(sizeof(struct rfsinfo));
	info->nodes = h;
	info->next_id = 1;

	fs->data = info;
	fs->root_inode_id = 0;
	fs->fs_ops = &ramfs_fsops;
	fs->fs_inode_ops = &ramfs_iops;
	hash_table_set_entry(h, &fs->root_inode_id, sizeof(fs->root_inode_id), 1, root);

	struct rfsdirent *dot = kmalloc(sizeof(struct rfsdirent));
	strncpy(dot->name, ".", 1);
	dot->namelen = 1;
	dot->ino = 0;
	dot->lnode = ll_insert(root->ents, dot);
	return 0;
}

int ramfs_alloc_inode(struct filesystem *fs, uint32_t *id)
{
	struct rfsinfo *info = fs->data;
	*id = add_atomic(&info->next_id, 1);
	kprintf("ALLOC: %d\n", *id);
	struct rfsnode *node = kmalloc(sizeof(struct rfsnode));
	node->num = *id;
	
	hash_table_set_entry(info->nodes, id, sizeof(*id), 1, node);
	return 0;
}

int ramfs_inode_push(struct filesystem *fs, struct inode *node)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;
	
	rfsnode->uid = node->uid;
	rfsnode->gid = node->gid;
	rfsnode->mode = node->mode;
	rfsnode->length = node->length;
	return 0;
}

int ramfs_inode_pull(struct filesystem *fs, struct inode *node)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;
	
	node->uid = rfsnode->uid;
	node->gid = rfsnode->gid;
	node->mode = rfsnode->mode;
	node->length = rfsnode->length;
	node->nlink = rfsnode->nlinks;
	node->id = rfsnode->num;
	kprintf("pulled id=%d\n", node->id);
	return 0;
}

int ramfs_inode_get_dirent(struct filesystem *fs, struct inode *node,
		const char *name, size_t namelen, struct dirent *dir)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;

	struct llistnode *ln;
	struct rfsdirent *rd, *found=0;
	rwlock_acquire(&rfsnode->ents->rwl, RWL_READER);
	ll_for_each_entry(rfsnode->ents, ln, struct rfsdirent *, rd) {
		if(!strncmp(rd->name, name, namelen) && namelen == rd->namelen) {
			found = rd;
			break;
		}
	}
	rwlock_release(&rfsnode->ents->rwl, RWL_READER);
	if(!found)
		return -ENOENT;

	dir->ino = found->ino;
	memcpy(dir->name, found->name, namelen);
	dir->namelen = namelen;
	return 0;
}

int ramfs_inode_readdir(struct filesystem *fs, struct inode *node, size_t num, struct dirent *dir)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;

	struct llistnode *ln;
	struct rfsdirent *rd, *found=0;
	rwlock_acquire(&rfsnode->ents->rwl, RWL_READER);
	ll_for_each_entry(rfsnode->ents, ln, struct rfsdirent *, rd) {
		if(num-- == 0) {
			found = rd;
			break;
		}
	}
	rwlock_release(&rfsnode->ents->rwl, RWL_READER);
	if(!found)
		return -ENOENT;

	dir->ino = found->ino;
	memcpy(dir->name, found->name, found->namelen);
	dir->namelen = found->namelen;
	return 0;
}

int ramfs_inode_link(struct filesystem *fs, struct inode *parent, struct inode *target,
		const char *name, size_t namelen)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsparent, *rfstarget;
	kprintf("LINK %s -> %d int %d\n", name, target->id, parent->id);
	if(hash_table_get_entry(info->nodes, &parent->id, sizeof(parent->id), 1, (void **)&rfsparent))
		return -EIO;

	if(hash_table_get_entry(info->nodes, &target->id, sizeof(target->id), 1, (void **)&rfstarget))
		return -EIO;

	add_atomic(&rfstarget->nlinks, 1);
	struct rfsdirent *dir = kmalloc(sizeof(struct rfsdirent));
	dir->ino = target->id;
	memcpy(dir->name, name, namelen);
	dir->namelen = namelen;

	dir->lnode = ll_insert(rfsparent->ents, dir);
	return 0;
}

int ramfs_inode_unlink(struct filesystem *fs, struct inode *parent, const char *name,
		size_t namelen)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsparent, *rfstarget;
	if(hash_table_get_entry(info->nodes, &parent->id, sizeof(parent->id), 1, (void **)&rfsparent))
		return -EIO;
	
	struct llistnode *ln;
	struct rfsdirent *rd, *found=0;
	rwlock_acquire(&rfsparent->ents->rwl, RWL_READER);
	ll_for_each_entry(rfsparent->ents, ln, struct rfsdirent *, rd) {
		if(!strncmp(rd->name, name, namelen) && namelen == rd->namelen) {
			found = rd;
			break;
		}
	}
	rwlock_release(&rfsparent->ents->rwl, RWL_READER);
	if(!found)
		return -ENOENT;

	if(hash_table_get_entry(info->nodes, &found->ino, sizeof(found->ino), 1, (void **)&rfstarget))
		return -EIO;

	found->ino = 0;
	int r = sub_atomic(&rfstarget->nlinks, 1);
	if(!r) {
		hash_table_delete_entry(info->nodes, &found->ino, sizeof(found->ino), 1);
		kfree(rfstarget);
	}

	ll_remove(rfsparent->ents, found->lnode);
	kfree(found);
	return 0;
}

int ramfs_inode_read(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, char *buffer)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	size_t amount = length;
	if(offset + length > node->length)
		amount = node->length - offset;
	if(!amount || offset > node->length)
		return 0;

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;

	memcpy(buffer, (unsigned char *)rfsnode->data + offset, amount);
	return amount;
}

int ramfs_inode_write(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, const char *buffer)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	size_t end = length + offset;
#warning "locking"
	if(end > node->length) {
		void *newdata = kmalloc(end);
		memcpy(newdata, rfsnode->data, rfsnode->length);
		kfree(rfsnode->data);
		rfsnode->data = newdata;
		rfsnode->length = end;
	}

	if(hash_table_get_entry(info->nodes, &node->id, sizeof(node->id), 1, (void **)&rfsnode))
		return -EIO;

	memcpy((unsigned char *)rfsnode->data + offset, buffer, length);
	return length;
}

struct filesystem_inode_callbacks ramfs_iops = {
	.push = ramfs_inode_push,
	.pull = ramfs_inode_pull,
	.read = ramfs_inode_read,
	.write = ramfs_inode_write,
	.lookup = ramfs_inode_get_dirent,
	.readdir = ramfs_inode_readdir,
	.link = ramfs_inode_link,
	.unlink = ramfs_inode_unlink,
	.select = 0
};

struct filesystem_callbacks ramfs_fsops = {
	.alloc_inode = ramfs_alloc_inode,
};

#if 0
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
#endif
