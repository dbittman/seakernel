#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int write_fs(struct inode *i, off_t off, size_t len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(is_directory(i))
		return -EISDIR;
	if(!permissions(i, MAY_WRITE))
		return -EACCES;
	return vfs_callback_write(i, off, len, b);
}

int read_fs(struct inode *i, off_t off, size_t  len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(!permissions(i, MAY_READ))
		return -EACCES;
	return vfs_callback_read(i, off, len, b);
}
