/* Provides functions for read/write/ctl of char devices */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/dm/char.h>
#include <sea/tty/terminal.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
static struct mutex cd_search_lock;

static int zero_rw(int rw, int m, char *buf, size_t c)
{
	m=0;
	if(rw == READ)
		memset(buf, 0, c);
	else if(rw == WRITE)
		return 0;
	return c;
}

static int null_rw(int rw, int m, char *buf, size_t c)
{
	if((m=rw) == READ)
		return 0;
	return c;
}

/* Built in devices:
	0 -> Null
	1 -> Zero
	2 -> Mem
	3 -> ttyx
	4 -> tty
	5 -> serial
	6 - 9 -> reserved
*/

struct chardevice *dm_set_char_device(int maj,
		ssize_t (*rw)(int, struct file *, off_t off, char*, size_t),
		int (*c)(struct file *, int, long),
		int (*s)(struct file *, int))
{
	printk(1, "[dev]: Setting char device %d (%x, %x)\n", maj, rw, c);
	struct chardevice *dev = (struct chardevice *)kmalloc(sizeof(struct chardevice));
	dev->func = rw;
	dev->ioctl=c;
	dev->select=s;
	dm_add_device(DT_CHAR, maj, dev);
	return dev;
}

int dm_set_available_char_device( 
		ssize_t (*rw)(int, struct file *, off_t off, char*, size_t),
		int (*c)(struct file *, int, long),
		int (*s)(struct file *, int))
{
	int i=10; /* first 10 character devices are reserved */
	mutex_acquire(&cd_search_lock);
	while(i>0) {
		if(!dm_get_device(DT_CHAR, i))
		{
			dm_set_char_device(i, rw, c, s);
			break;
		}
		i++;
	}
	mutex_release(&cd_search_lock);
	if(i < 0)
		return -EINVAL;
	return i;
}

void dm_init_char_devices(void)
{
	/* These devices are all built into the kernel. We must initialize them now */
	dm_set_char_device(0, null_rw, 0, 0);
	dm_set_char_device(1, zero_rw, 0, 0);
	dm_set_char_device(3, ttyx_rw, ttyx_ioctl, ttyx_select);
	dm_set_char_device(4, tty_rw, tty_ioctl, tty_select);
	dm_set_char_device(5, serial_rw, 0, 0);
	dm_set_char_device(6, net_char_rw, net_char_ioctl, net_char_select);
	mutex_create(&cd_search_lock, 0);
}

int dm_char_rw(int rw, struct file *file, off_t off, char *buf, size_t len)
{
	struct device *dt = dm_get_device(DT_CHAR, MAJOR(file->inode->phys_dev));
	if(!dt)
		return -ENXIO;
	struct chardevice *cd = (struct chardevice *)dt->ptr;
	if(cd->func)
	{
		int ret = (cd->func)(rw, file, off, buf, len);
		return ret;
	}
	return 0;
}

void dm_unregister_char_device(int n)
{
	printk(1, "[dev]: Unregistering char device %d\n", n);
	mutex_acquire(&cd_search_lock);
	struct device *dev = dm_get_device(DT_CHAR, n);
	if(!dev) {
		mutex_release(&cd_search_lock);
		return;
	}
	void *fr = dev->ptr;
	dev->ptr=0;
	dm_remove_device(DT_CHAR, n);
	mutex_release(&cd_search_lock);
	kfree(fr);
}

int dm_char_ioctl(struct file *file, int cmd, long arg)
{
	struct device *dt = dm_get_device(DT_CHAR, MAJOR(file->inode->phys_dev));
	if(!dt)
		return -ENXIO;
	struct chardevice *cd = (struct chardevice *)dt->ptr;
	if(cd->ioctl)
	{
		int ret = (cd->ioctl)(file, cmd, arg);
		return ret;
	}
	return 0;
}

int dm_chardev_select(struct file *f, int rw)
{
	int dev = f->inode->phys_dev;
	struct device *dt = dm_get_device(DT_CHAR, MAJOR(dev));
	if(!dt)
		return 1;
	struct chardevice *cd = (struct chardevice *)dt->ptr;
	if(cd->select)
		return cd->select(f, rw);
	return 1;
}

void dm_send_sync_char(void)
{
}
