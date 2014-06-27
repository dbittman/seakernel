#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
int vfs_write_inode(struct inode *i, off_t off, size_t len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(vfs_inode_is_directory(i))
		return -EISDIR;
	if(!vfs_inode_get_check_permissions(i, MAY_WRITE, 0))
		return -EACCES;
	int r = vfs_callback_write(i, off, len, b);
	i->mtime = arch_time_get_epoch();
	sync_inode_tofs(i);
	return r;
}

int vfs_read_inode(struct inode *i, off_t off, size_t  len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(!vfs_inode_get_check_permissions(i, MAY_READ, 0))
		return -EACCES;
	return vfs_callback_read(i, off, len, b);
}
