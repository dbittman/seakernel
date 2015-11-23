#include <sea/kernel.h>
#include <sea/dm/char.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/sys/stat.h>
#include <sea/dm/block.h>
#include <sea/loader/symbol.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>

#define MAX_DEVICES 256
static struct kdevice *devices[MAX_DEVICES];
static struct mutex lock;
int dm_device_register(struct kdevice *dev)
{
	mutex_acquire(&lock);
	for(int i=0;i<MAX_DEVICES;i++) {
		if(!devices[i]) {
			devices[i] = dev;
			dev->refs = 0;
			mutex_release(&lock);
			return i;
		}
	}
	mutex_release(&lock);
	return -1;
}

struct kdevice *dm_device_get(int major)
{
	mutex_acquire(&lock);
	struct kdevice *dev = devices[major];
	if(dev) {
		atomic_fetch_add(&dev->refs, 1);
	}
	mutex_release(&lock);
	return dev;
}

void dm_device_put(struct kdevice *kdev)
{
	atomic_fetch_sub(&kdev->refs, 1);
}

void dm_init(void)
{
	printk(KERN_DEBUG, "[dev]: Loading device management...\n");
	mutex_create(&lock, 0);
	memset(devices, 0, sizeof(devices));
#if CONFIG_MODULES
	loader_add_kernel_symbol(dm_device_get);
	loader_add_kernel_symbol(dm_device_put);
	loader_add_kernel_symbol(dm_device_register);
#endif
}

