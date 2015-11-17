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

extern void dm_char_register(); //TODO

static struct devhash_s devhash[NUM_DT];

struct device *dm_get_device(int type, int major)
{
	if(type >= NUM_DT)
		return 0;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	struct device *dt = devhash[type].devs[alpha];
	while(dt && dt->beta != beta) 
		dt=dt->next;
	mutex_release(&devhash[type].lock);
	if(dt && !dt->ptr) return 0;
	return dt;
}

struct device *dm_get_enumerated_device(int type, int n)
{
	if(type >= NUM_DT)
		return 0;
	int a=0;
	struct device *dt=0;
	mutex_acquire(&devhash[type].lock);
	while(!dt && a < DH_SZ)
	{
		dt = devhash[type].devs[a];
		while(n && dt) {
			dt=dt->next;
			n--;
		}
		a++;
	}
	mutex_release(&devhash[type].lock);
	if(dt && !dt->ptr) return 0;
	return dt;
}

int dm_add_device(int type, int major, void *str)
{
	if(type >= NUM_DT)
		return -1;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	struct device *new = (struct device *)kmalloc(sizeof(struct device));
	new->beta = beta;
	new->ptr = str;
	struct device *old = devhash[type].devs[alpha];
	devhash[type].devs[alpha] = new;
	new->next = old;
	mutex_release(&devhash[type].lock);
	return 0;
}

int dm_remove_device(int type, int major)
{
	if(type >= NUM_DT)
		return -1;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	struct device *d = devhash[type].devs[alpha];
	struct device *p = 0;
	while(d && d->beta != beta) {
		p=d;
		d=d->next;
	}
	if(d){
		if(!p) {
			assert(devhash[type].devs[alpha] == d);
			devhash[type].devs[alpha] = d->next;
		}
		else {
			assert(p->next == d);
			p->next = d->next;
		}
		kfree(d);
	}
	mutex_release(&devhash[type].lock);
	return 0;
}

int dm_ioctl(int type, dev_t dev, int cmd, long arg)
{
	if(S_ISBLK(type))
		return dm_block_ioctl(dev, cmd, arg);
	else
		return -EINVAL;
	return 0;
}

void dm_sync(void)
{
	dm_send_sync_block();
}

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
	memset(devhash, 0, sizeof(struct devhash_s)*NUM_DT);
	int i;
	for(i=0;i<NUM_DT;i++) 
		mutex_create(&devhash[i].lock, 0);
	dm_init_block_devices();
#if CONFIG_MODULES
	//loader_add_kernel_symbol(dm_block_rw);
	loader_add_kernel_symbol(dm_block_ioctl);
	loader_add_kernel_symbol(dm_block_device_rw);
	loader_add_kernel_symbol(dm_set_available_block_device);
	loader_add_kernel_symbol(dm_device_get);
	loader_add_kernel_symbol(dm_device_put);
	loader_add_kernel_symbol(dm_device_register);
	loader_add_kernel_symbol(dm_unregister_block_device);
	loader_add_kernel_symbol(dm_get_device);
	loader_add_kernel_symbol(dm_block_read);
	loader_add_kernel_symbol(dm_blockdev_select);
	loader_add_kernel_symbol(dm_block_device_select);
	loader_add_kernel_symbol(dm_do_block_rw);
	loader_add_kernel_symbol(dm_do_block_rw_multiple);
	loader_add_kernel_symbol(dm_block_write);
	loader_add_kernel_symbol(dm_set_block_device);
#endif
}

