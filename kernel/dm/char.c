/* Provides functions for read/write/ctl of char devices */
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
static ssize_t zero_rw(int rw, struct file *file, off_t m, uint8_t *buf, size_t c)
{
	(void)m;
	(void)file;
	if(rw == READ)
		memset(buf, 0, c);
	else if(rw == WRITE)
		return 0;
	return c;
}

static ssize_t null_rw(int rw, struct file *file, off_t m, uint8_t *buf, size_t c)
{
	(void)m;
	(void)file;
	if(rw == READ)
		return 0;
	return c;
}

static struct kdevice __zero_kdev = {
	.select = 0,.open = 0,.close = 0,.destroy = 0,.create = 0,.ioctl = 0,
	.name = "zero",
	.rw = zero_rw,
};

static struct kdevice __null_kdev = {
	.select = 0,.open = 0,.close = 0,.destroy = 0,.create = 0,.ioctl = 0,
	.name = "null",
	.rw = null_rw,
};

void dm_init_char_devices(void)
{
	int zero = dm_device_register(&__zero_kdev);
	int null = dm_device_register(&__null_kdev);
	sys_mknod("/dev/null", S_IFCHR | 0666, GETDEV(null, 0));
	sys_mknod("/dev/zero", S_IFCHR | 0666, GETDEV(zero, 0));
}

