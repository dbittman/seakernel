#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>
#include <sea/fs/inode.h>

int vfs_write_inode(struct inode *i, off_t off, size_t len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(vfs_inode_is_directory(i))
		return -EISDIR;
	if(!vfs_inode_get_check_permissions(i, MAY_WRITE))
		return -EACCES;
	return vfs_callback_write(i, off, len, b);
}

int vfs_read_inode(struct inode *i, off_t off, size_t  len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(!vfs_inode_get_check_permissions(i, MAY_READ))
		return -EACCES;
	return vfs_callback_read(i, off, len, b);
}
