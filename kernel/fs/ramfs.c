#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/fs/fs.h>
#include <sea/lib/hash.h>
#include <sea/mm/kmalloc.h>
#include <sea/fs/dir.h>
#include <stdatomic.h>
#include <sea/lib/linkedlist.h>
#include <sea/errno.h>
#include <sea/dm/dev.h>

struct filesystem_inode_callbacks ramfs_iops;
struct filesystem_callbacks ramfs_fsops;

struct rfsdirent {
	char name[DNAME_LEN];
	size_t namelen;
	uint32_t ino;
	struct linkedentry lnode;
};

struct rfsnode {
	void *data;
	dev_t dev;
	size_t length;
	uint32_t num;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	uint16_t nlinks;
	struct linkedlist ents;
	struct hashelem hash_elem;
};

struct rfsinfo {
	struct hash *nodes;
	uint32_t next_id;
};

int ramfs_mount(struct filesystem *fs)
{
	struct hash *h = hash_create(0, 0, 1000);
	struct rfsnode *root = kmalloc(sizeof(struct rfsnode));
	root->mode = S_IFDIR | 0755;
	linkedlist_create(&root->ents, LINKEDLIST_MUTEX);
	root->nlinks = 2;

	struct rfsinfo *info = kmalloc(sizeof(struct rfsinfo));
	info->nodes = h;
	info->next_id = 1;

	fs->data = info;
	fs->root_inode_id = 0;
	fs->fs_ops = &ramfs_fsops;
	fs->fs_inode_ops = &ramfs_iops;
	hash_insert(h, &root->num, sizeof(root->num), &root->hash_elem, root);

	struct rfsdirent *dot = kmalloc(sizeof(struct rfsdirent));
	strncpy(dot->name, ".", 1);
	dot->namelen = 1;
	dot->ino = 0;
	linkedlist_insert(&root->ents, &dot->lnode, dot);
	dot = kmalloc(sizeof(struct rfsdirent));
	strncpy(dot->name, "..", 2);
	dot->namelen = 2;
	dot->ino = 0;
	linkedlist_insert(&root->ents, &dot->lnode, dot);
	return 0;
}

int ramfs_alloc_inode(struct filesystem *fs, uint32_t *id)
{
	struct rfsinfo *info = fs->data;
	*id = atomic_fetch_add(&info->next_id, 1) + 1;
	struct rfsnode *node = kmalloc(sizeof(struct rfsnode));
	node->num = *id;
	linkedlist_create(&node->ents, LINKEDLIST_MUTEX);

	hash_insert(info->nodes, &node->num, sizeof(node->num), &node->hash_elem, node);
	return 0;
}

int ramfs_inode_push(struct filesystem *fs, struct inode *node)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;

	rfsnode->uid = node->uid;
	rfsnode->gid = node->gid;
	rfsnode->mode = node->mode;
	rfsnode->length = node->length;
	rfsnode->dev = node->phys_dev;
	return 0;
}

int ramfs_inode_pull(struct filesystem *fs, struct inode *node)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;

	node->uid = rfsnode->uid;
	node->gid = rfsnode->gid;
	node->mode = rfsnode->mode;
	node->length = rfsnode->length;
	node->nlink = rfsnode->nlinks;
	node->id = rfsnode->num;
	node->phys_dev = rfsnode->dev;
	return 0;
}

int ramfs_inode_get_dirent(struct filesystem *fs, struct inode *node,
		const char *name, size_t namelen, struct dirent *dir)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;

	struct linkedentry *ln;
	struct rfsdirent *rd, *found=0;
	__linkedlist_lock(&rfsnode->ents);
	for(ln = linkedlist_iter_start(&rfsnode->ents); ln != linkedlist_iter_end(&rfsnode->ents);
			ln = linkedlist_iter_next(ln)) {
		rd = linkedentry_obj(ln);
		if(!strncmp(rd->name, name, namelen) && namelen == rd->namelen) {
			found = rd;
			break;
		}
	}
	__linkedlist_unlock(&rfsnode->ents);
	if(!found)
		return -ENOENT;

	dir->ino = found->ino;
	memcpy(dir->name, found->name, namelen);
	dir->namelen = namelen;
	return 0;
}

int __get_dirent_type(struct filesystem *fs, struct rfsdirent *rd)
{
	struct inode node;
	node.id = rd->ino;
	ramfs_inode_pull(fs, &node);
	if(S_ISDIR(node.mode)) {
		return DT_DIR;
	} else if(S_ISREG(node.mode)) {
		return DT_REG;
	} else if(S_ISLNK(node.mode)) {
		return DT_LNK;
	} else if(S_ISFIFO(node.mode)) {
		return DT_FIFO;
	} else if(S_ISSOCK(node.mode)) {
		return DT_SOCK;
	} else if(S_ISCHR(node.mode)) {
		return DT_CHR;
	} else if(S_ISBLK(node.mode)) {
		return DT_BLK;
	} else {
		return DT_UNKNOWN;
	}
}

int ramfs_inode_getdents(struct filesystem *fs, struct inode *node, unsigned off, struct dirent_posix *dirs,
		unsigned count, unsigned *nextoff)
{
	unsigned read = 0;
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;

	struct linkedentry *ln;
	unsigned char *rec = (void *)dirs;
	struct rfsdirent *rd, *found=0;
	__linkedlist_lock(&rfsnode->ents);
	for(ln = linkedlist_iter_start(&rfsnode->ents); ln != linkedlist_iter_end(&rfsnode->ents);
			ln = linkedlist_iter_next(ln)) {
		rd = linkedentry_obj(ln);

		int reclen = rd->namelen + sizeof(struct dirent_posix) + 1;
		reclen &= ~15;
		reclen += 16;
		if(read >= off) {
			if(reclen + (read - off) > count)
				break;
			struct dirent_posix *dp = (void *)rec;
			dp->d_reclen = reclen;
			memcpy(dp->d_name, rd->name, rd->namelen);
			dp->d_name[rd->namelen]=0;
			dp->d_off = read + reclen;
			*nextoff = dp->d_off;
			dp->d_type = __get_dirent_type(fs, rd);
			dp->d_ino = rd->ino;

			rec += reclen;
		}
		read += reclen;
	}
	__linkedlist_unlock(&rfsnode->ents);

	return (addr_t)rec - (addr_t)dirs;
}

int ramfs_inode_link(struct filesystem *fs, struct inode *parent, struct inode *target,
		const char *name, size_t namelen)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsparent, *rfstarget;
	if((rfsparent = hash_lookup(info->nodes, &parent->id, sizeof(parent->id))) == NULL)
		return -EIO;

	if((rfstarget = hash_lookup(info->nodes, &target->id, sizeof(target->id))) == NULL)
		return -EIO;

	atomic_fetch_add_explicit(&rfstarget->nlinks, 1, memory_order_relaxed);
	struct rfsdirent *dir = kmalloc(sizeof(struct rfsdirent));
	dir->ino = target->id;
	memcpy(dir->name, name, namelen);
	dir->namelen = namelen;
	parent->length += 512;

	linkedlist_insert(&rfsparent->ents, &dir->lnode, dir);
	return 0;
}

int ramfs_inode_unlink(struct filesystem *fs, struct inode *parent, const char *name,
		size_t namelen, struct inode *target)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsparent, *rfstarget;
	if((rfsparent = hash_lookup(info->nodes, &parent->id, sizeof(parent->id))) == NULL)
		return -EIO;

	struct linkedentry *ln;
	struct rfsdirent *rd, *found=0;
	__linkedlist_lock(&rfsparent->ents);
	for(ln = linkedlist_iter_start(&rfsparent->ents);
			ln != linkedlist_iter_end(&rfsparent->ents);
			ln = linkedlist_iter_next(ln)) {
		rd = linkedentry_obj(ln);
		if(!strncmp(rd->name, name, namelen) && namelen == rd->namelen) {
			found = rd;
			break;
		}
	}
	__linkedlist_unlock(&rfsparent->ents);

	if(!found)
		return -ENOENT;

	if((rfstarget = hash_lookup(info->nodes, &found->ino, sizeof(found->ino))) == NULL)
		return -EIO;

	found->ino = 0;
	int r = atomic_fetch_sub_explicit(&rfstarget->nlinks, 1, memory_order_relaxed);
	if(r == 1) {
		hash_delete(info->nodes, &target->id, sizeof(target->id));
		linkedlist_destroy(&rfstarget->ents);
		kfree(rfstarget);
	}

	linkedlist_remove(&rfsparent->ents, &found->lnode);
	kfree(found);
	return 0;
}

int ramfs_inode_read(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, unsigned char *buffer)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	size_t amount = length;
	if(offset + length > node->length)
		amount = node->length - offset;
	if(!amount || offset > node->length)
		return 0;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;

	memcpy(buffer, (unsigned char *)rfsnode->data + offset, amount);
	return amount;
}

void ramfs_point_to_data(struct inode *node, void *data, size_t len)
{
	struct rfsinfo *info = node->filesystem->data;
	struct rfsnode *rfsnode;
	
	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return;

	if((addr_t)rfsnode->data >= MEMMAP_KMALLOC_START && (addr_t)rfsnode->data < MEMMAP_KMALLOC_END)
		kfree(rfsnode->data);
	rfsnode->data = data;
	rfsnode->length = node->length = len;
}

int ramfs_inode_write(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, const unsigned char *buffer)
{
	struct rfsinfo *info = fs->data;
	struct rfsnode *rfsnode;

	if((rfsnode = hash_lookup(info->nodes, &node->id, sizeof(node->id))) == NULL)
		return -EIO;
	size_t end = length + offset;
	if(end > node->length && end > 0x1000)
		return -EIO;
	rwlock_acquire(&node->metalock, RWL_WRITER);
	if(end > node->length) {
		void *newdata = kmalloc(end);
		if(rfsnode->data) {
			memcpy(newdata, rfsnode->data, rfsnode->length);
			if((addr_t)rfsnode->data >= MEMMAP_KMALLOC_START && (addr_t)rfsnode->data < MEMMAP_KMALLOC_END)
			{
				kfree(rfsnode->data);
			}
		}
		rfsnode->data = newdata;
		rfsnode->length = end;
		node->length = end;
	}
	rwlock_release(&node->metalock, RWL_WRITER);


	memcpy((unsigned char *)rfsnode->data + offset, buffer, length);
	return length;
}

extern struct filesystem *devfs;
int ramfs_fs_stat(struct filesystem *fs, struct posix_statfs *stat)
{
	if(fs == devfs)
		stat->f_type = 0x1373;
	else
		stat->f_type = 0x858458f6;
	stat->f_bsize  = 4096;
	stat->f_blocks = 0;
	stat->f_bfree  = 0;
	stat->f_bavail = 0;
	stat->f_files  = ((struct rfsinfo *)fs->data)->nodes->count;
	stat->f_ffree  = 0;
	stat->f_fsid   = fs->id;
	stat->f_namelen= 0;
	stat->f_frsize = 0;
	stat->f_flags  = 0;
	return 0;
}

struct filesystem_inode_callbacks ramfs_iops = {
	.push = ramfs_inode_push,
	.pull = ramfs_inode_pull,
	.read = ramfs_inode_read,
	.write = ramfs_inode_write,
	.lookup = ramfs_inode_get_dirent,
	.getdents = ramfs_inode_getdents,
	.link = ramfs_inode_link,
	.unlink = ramfs_inode_unlink,
	.select = 0
};

struct filesystem_callbacks ramfs_fsops = {
	.alloc_inode = ramfs_alloc_inode,
	.fs_stat     = ramfs_fs_stat,
};

