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
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->write)
		return node->filesystem->fs_inode_ops->write(node->filesystem, node, off, len, buf);
	return -ENOTSUP;
}

int fs_callback_inode_pull(struct inode *node)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->pull)
		return node->filesystem->fs_inode_ops->pull(node->filesystem, node);
	return -ENOTSUP;
}

int fs_callback_inode_push() {}

int fs_callback_inode_link(struct inode *node, struct inode *target, const char *name, size_t namelen)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->link)
		return node->filesystem->fs_inode_ops->link(node->filesystem, node,
				target, name, namelen);
	return -ENOTSUP;
}

int fs_callback_inode_unlink() {}

int fs_callback_inode_lookup(struct inode *node, const char *name,
		size_t namelen, struct dirent *dir)
{
	assert(node && node->filesystem);
	if(node->filesystem->fs_inode_ops->lookup)
		return node->filesystem->fs_inode_ops->lookup(node->filesystem, node, name, namelen, dir);
	return -ENOTSUP;
}

int fs_callback_inode_readdir() {}
int fs_callback_inode_select() {}

int fs_callback_fs_alloc_inode(struct filesystem *fs, uint32_t *id)
{
	assert(fs);
	if(fs->fs_ops->alloc_inode)
		return fs->fs_ops->alloc_inode(fs, id);
	return -ENOTSUP;
}

int fs_callback_fs_stat() {}

