#include <kernel.h>
#include <char.h>
#include <dev.h>
#include <fs.h>
#include <sys/stat.h>
#include <block.h>

struct devhash_s devhash[NUM_DT];

void init_dm()
{
	printk(KERN_DEBUG, "[dev]: Loading device management...\n");
	memset(devhash, 0, sizeof(struct devhash_s)*NUM_DT);
	int i;
	for(i=0;i<NUM_DT;i++) 
		mutex_create(&devhash[i].lock);
	init_char_devs();
	init_block_devs();
}

device_t *get_device(int type, int major)
{
	if(type >= NUM_DT)
		return 0;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	device_t *dt = devhash[type].devs[alpha];
	while(dt && dt->beta != beta) 
		dt=dt->next;
	mutex_release(&devhash[type].lock);
	if(!dt->ptr) return 0;
	return dt;
}

device_t *get_n_device(int type, int n)
{
	if(type >= NUM_DT)
		return 0;
	int a=0;
	device_t *dt=0;
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
	if(!dt->ptr) return 0;
	return dt;
}

int add_device(int type, int major, void *str)
{
	if(type >= NUM_DT)
		return -1;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	device_t *new = (device_t *)kmalloc(sizeof(device_t));
	new->beta = beta;
	new->ptr = str;
	device_t *old = devhash[type].devs[alpha];
	devhash[type].devs[alpha] = new;
	new->next = old;
	mutex_release(&devhash[type].lock);
	return 0;
}

int remove_device(int type, int major)
{
	if(type >= NUM_DT)
		return -1;
	int alpha = major % DH_SZ;
	int beta = major / DH_SZ;
	mutex_acquire(&devhash[type].lock);
	device_t *d = devhash[type].devs[alpha];
	device_t *p = 0;
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

int dm_ioctl(int type, dev_t dev, int cmd, int arg)
{
	if(S_ISCHR(type))
		return char_ioctl(dev, cmd, arg);
	else if(S_ISBLK(type))
		return block_ioctl(dev, cmd, arg);
	else
		return -EINVAL;
	return 0;
}

void sync_dm()
{
	send_sync_block();
}
