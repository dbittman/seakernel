#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int read_fs(struct inode *i, int off, int len, char *b)
{
	if(!i || !b)
		return -EINVAL;
	if(!permissions(i, MAY_READ))
		return -EACCES;
	if(i->i_ops && i->i_ops->read)
		return i->i_ops->read(i, off, len, b);
	return -EINVAL;
}
