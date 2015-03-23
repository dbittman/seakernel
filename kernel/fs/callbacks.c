#include <sea/fs/inode.h>
#include <sea/errno.h>

int fs_callback_inode_read(struct inode *node, size_t off, size_t len, char *buf)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->read)
		return node->filesystem->fs_inode_ops->read(node->filesystem, node, off, len, buf);
	return -ENOTSUP;
}

int fs_callback_inode_write(struct inode *node, size_t off, size_t len, const char *buf)
{
	assert(node && node->filesystem && node->filesystem->fs_inode_ops);
	if(node->filesystem->fs_inode_ops->write)
		return node->filesystem->fs_inode_ops->write(node->filesystem, node, off, len, buf);
	return -ENOTSUP;
}

int fs_callback_inode_pull(struct inode *node)
{
	assert(node && node->filesystem && node->filesystem->fs_inode_ops);
	if(node->filesystem->fs_inode_ops->pull)
		return node->filesystem->fs_inode_ops->pull(node->filesystem, node);
	return -ENOTSUP;
}

int fs_callback_inode_push(struct inode *node)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->push)
		return node->filesystem->fs_inode_ops->push(node->filesystem, node);
	return -ENOTSUP;
}

int fs_callback_inode_link(struct inode *node, struct inode *target, const char *name, size_t namelen)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->link)
		return node->filesystem->fs_inode_ops->link(node->filesystem, node,
				target, name, namelen);
	return -ENOTSUP;
}

int fs_callback_inode_unlink(struct inode *node, const char *name, size_t namelen, struct inode * target)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->unlink)
		return node->filesystem->fs_inode_ops->unlink(node->filesystem, node, name, namelen, target);
	return -ENOTSUP;
}

int fs_callback_inode_lookup(struct inode *node, const char *name,
		size_t namelen, struct dirent *dir)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->lookup)
		return node->filesystem->fs_inode_ops->lookup(node->filesystem, node, name, namelen, dir);
	return -ENOTSUP;
}

int fs_callback_inode_getdents(struct inode *node, unsigned off, struct dirent_posix *ds, unsigned count, unsigned *nextoff)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->getdents)
		return node->filesystem->fs_inode_ops->getdents(node->filesystem, node, off, ds, count, nextoff);
	return -ENOTSUP;
}

int fs_callback_inode_select(struct inode *node, int rw)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->select)
		return node->filesystem->fs_inode_ops->select(node->filesystem, node, rw);
	return -ENOTSUP;
}

int fs_callback_fs_alloc_inode(struct filesystem *fs, uint32_t *id)
{
	assert(fs);
	if(fs->fs_ops->alloc_inode)
		return fs->fs_ops->alloc_inode(fs, id);
	return -ENOTSUP;
}

int fs_callback_fs_stat(struct filesystem *fs, struct posix_statfs *p)
{
	assert(fs);
	if(fs->fs_ops->fs_stat)
		return fs->fs_ops->fs_stat(fs, p);
	return -ENOTSUP;
}

