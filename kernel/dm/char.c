/* Provides functions for read/write/ctl of char devices */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/dm/char.h>
#include <sea/tty/terminal.h>
mutex_t cd_search_lock;
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

chardevice_t *dm_set_char_device(int maj, int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int))
{
	printk(1, "[dev]: Setting char device %d (%x, %x)\n", maj, f, c);
	chardevice_t *dev = (chardevice_t *)kmalloc(sizeof(chardevice_t));
	dev->func = f;
	dev->ioctl=c;
	dev->select=s;
	dm_add_device(DT_CHAR, maj, dev);
	return dev;
}

int dm_set_available_char_device(int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int))
{
	int i=10; /* first 10 character devices are reserved */
	mutex_acquire(&cd_search_lock);
	while(i>0) {
		if(!dm_get_device(DT_CHAR, i))
		{
			dm_set_char_device(i, f, c, s);
			break;
		}
		i++;
	}
	mutex_release(&cd_search_lock);
	if(i < 0)
		return -EINVAL;
	return i;
}

void dm_init_char_devices()
{
	/* These devices are all built into the kernel. We must initialize them now */
	dm_set_char_device(0, null_rw, 0, 0);
	dm_set_char_device(1, zero_rw, 0, 0);
	dm_set_char_device(3, ttyx_rw, ttyx_ioctl, ttyx_select);
	dm_set_char_device(4, tty_rw, tty_ioctl, tty_select);
	dm_set_char_device(5, serial_rw, 0, 0);
	mutex_create(&cd_search_lock, 0);
}

int dm_char_rw(int rw, dev_t dev, char *buf, size_t len)
{
	device_t *dt = dm_get_device(DT_CHAR, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	chardevice_t *cd = (chardevice_t *)dt->ptr;
	if(cd->func)
	{
		int ret = (cd->func)(rw, MINOR(dev), buf, len);
		return ret;
	}
	return 0;
}

void dm_unregister_char_device(int n)
{
	printk(1, "[dev]: Unregistering char device %d\n", n);
	mutex_acquire(&cd_search_lock);
	device_t *dev = dm_get_device(DT_CHAR, n);
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

int dm_char_ioctl(dev_t dev, int cmd, long arg)
{
	device_t *dt = dm_get_device(DT_CHAR, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	chardevice_t *cd = (chardevice_t *)dt->ptr;
	if(cd->ioctl)
	{
		int ret = (cd->ioctl)(MINOR(dev), cmd, arg);
		return ret;
	}
	return 0;
}

int dm_chardev_select(struct inode *in, int rw)
{
	int dev = in->dev;
	device_t *dt = dm_get_device(DT_CHAR, MAJOR(dev));
	if(!dt)
		return 1;
	chardevice_t *cd = (chardevice_t *)dt->ptr;
	if(cd->select)
		return cd->select(MINOR(dev), rw);
	return 1;
}

void dm_send_sync_char()
{
	int i=0;
	while(i>=0) {
		device_t *d = dm_get_enumerated_device(DT_CHAR, i);
		if(!d) break;
		assert(d->ptr);
		chardevice_t *cd = (chardevice_t *)d->ptr;
		if(cd->ioctl)
			cd->ioctl(0, -1, 0);
		i++;
		if(tm_process_got_signal(current_task))
			return;
	}
}
