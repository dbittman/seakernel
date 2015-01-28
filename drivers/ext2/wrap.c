#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/errno.h>
#include <modules/ext2.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>
#include <sea/fs/fs.h>
#include <sea/cpu/atomic.h>

int ext2_wrap_alloc_inode(struct filesystem *fs, uint32_t *id)
{
	struct ext2_info *info = fs->data;
	ext2_inode_t in;
	int r = ext2_inode_alloc(info, &in);
	if(!r)
		return -EOVERFLOW;
	*id = in.number;
	return 0;
}

int ext2_wrap_inode_push(struct filesystem *fs, struct inode *out)
{
	ext2_inode_t in;
	struct ext2_info *info = fs->data;
	if(!ext2_inode_read(info, out->id, &in))
		return -EIO;
	if(in.size > (unsigned)out->length)
		ext2_inode_truncate(&in, out->length, 0);
	in.mode = out->mode;
	in.uid = out->uid;
	in.size = out->length;
	in.access_time = out->atime;
	in.modification_time = out->mtime;
	in.change_time = out->ctime;
	in.gid = out->gid;
	ext2_inode_update(&in);
	return 0;
}

int ext2_wrap_inode_pull(struct filesystem *fs, struct inode *out)
{
	ext2_inode_t in;
	struct ext2_info *info = fs->data;
	if(!ext2_inode_read(info, out->id, &in))
		return -EIO;
	out->mode = in.mode;
	out->uid = in.uid;
	out->length = in.size;
	out->atime = in.access_time;
	out->mtime = in.modification_time;
	out->ctime = in.change_time;
	out->gid = in.gid;
	out->nlink = in.link_count;
	out->id = in.number;
	out->nblocks = in.sector_count;
	return 0;
}

int ext2_wrap_inode_get_dirent(struct filesystem *fs, struct inode *node,
		const char *name, size_t namelen, struct dirent *dir)
{
	struct ext2_info *info = fs->data;
	ext2_inode_t inode;
	ext2_inode_read(info, node->id, &inode);
	int ret = ext2_dir_get_inode(&inode, name, namelen);
	if(!ret) 
		return -ENOENT;
	strncpy(dir->name, name, namelen);
	dir->name[namelen]=0;
	dir->namelen = namelen;
	dir->ino = ret;
	return 0;
}

int ext2_wrap_inode_getdents(struct filesystem *fs, struct inode *node, unsigned off, struct dirent_posix *dirs,
		unsigned count, unsigned *nextoff)
{
	struct ext2_info *info = fs->data;
	ext2_inode_t dir;
	if(!ext2_inode_read(info, node->id, &dir))
		return -EIO;
	int ret = ext2_dir_getdents(&dir, off, dirs, count, nextoff);
	return ret;
}

int ext2_wrap_inode_link(struct filesystem *fs, struct inode *parent, struct inode *target,
		const char *name, size_t namelen)
{
	struct ext2_info *info = fs->data;
	if(info->flags & EXT2_FS_READONLY)
		return -EROFS;
	ext2_inode_t dir, tar;
	if(!ext2_inode_read(info, parent->id, &dir))
		return -EIO;
	int ret = ext2_dir_addent(&dir, target->id, ext2_inode_type(target->mode), name, namelen);
	if(ret) {
		ext2_inode_update(&dir);
		ext2_inode_read(info, target->id, &tar);
		tar.link_count++;
		if(tar.link_count == 1 && S_ISDIR(target->mode))
			ext2_dir_change_dir_count(&tar, 0);
		ext2_inode_update(&tar);
		add_atomic(&target->nlink, 1);
		parent->length = dir.size;
		parent->nblocks = dir.sector_count;
	}
	return ret == 0 ? -EINVAL : 0;
}

int ext2_wrap_inode_unlink(struct filesystem *fs, struct inode *parent, const char *name,
		size_t namelen)
{
	struct ext2_info *info = fs->data;
	
	if(info->flags & EXT2_FS_READONLY)
		return -EROFS;
	ext2_inode_t inode;
	if(!ext2_inode_read(info, parent->id, &inode))
		return -EIO;
	int ret = ext2_dir_delent(&inode, name, namelen, 1);
	if(!ret)
		return -ENOENT;
	return 0;
}

int ext2_wrap_inode_read(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, char *buffer)
{
	struct ext2_info *info = fs->data;
	ext2_inode_t inode;
	if(!ext2_inode_read(info, node->id, &inode))
		return -EIO;
	if((unsigned)offset >= inode.size)
		return 0;
	/* this is needed in case we haven't pushed, and it's a symlink */
	inode.mode = node->mode;
	if((offset + length) >= (unsigned)node->length)
		length = node->length - offset;
	unsigned int ret = ext2_inode_readdata(&inode, offset, length, (unsigned char *)buffer);
	return (int)ret;
}

int ext2_wrap_inode_write(struct filesystem *fs, struct inode *node,
		size_t offset, size_t length, const char *buffer)
{
	struct ext2_info *info = fs->data;
	if(info->flags & EXT2_FS_READONLY)
		return -EROFS;
	ext2_inode_t inode;
	if(!ext2_inode_read(info, node->id, &inode))
		return -EIO;
	unsigned sz = inode.size;
	/* this is needed in case we haven't pushed, and it's a symlink */
	inode.mode = node->mode;
	unsigned sc = inode.sector_count;
	unsigned int ret = ext2_inode_writedata(&inode, offset, length, (unsigned char*)buffer);
	if(sz != inode.sector_count || sz != inode.size) {
		node->length = inode.size;
		node->nblocks = inode.sector_count;
	}
	if(ret > length) ret = length;
	return ret;
}

struct filesystem_inode_callbacks ext2_wrap_iops = {
	.push = ext2_wrap_inode_push,
	.pull = ext2_wrap_inode_pull,
	.read = ext2_wrap_inode_read,
	.write = ext2_wrap_inode_write,
	.lookup = ext2_wrap_inode_get_dirent,
	.getdents = ext2_wrap_inode_getdents,
	.link = ext2_wrap_inode_link,
	.unlink = ext2_wrap_inode_unlink,
	.select = 0
};

struct filesystem_callbacks ext2_wrap_fsops = {
	.alloc_inode = ext2_wrap_alloc_inode,
};

