#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int write_fs(struct inode *i, int off, int len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(is_directory(i))
		return -EISDIR;
	if(!permissions(i, MAY_WRITE))
		return -EACCES;
	if(i->i_ops && i->i_ops->f_ops && i->i_ops->f_ops->write)
		return i->i_ops->f_ops->write(i, off, len, b);
	return -EINVAL;
}
