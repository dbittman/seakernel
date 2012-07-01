#include <kernel.h>
#include <fs.h>
#include <sys/stat.h>
#include <dev.h>
#include <cache.h>
#include <block.h>
#include <mod.h>
struct loop_device {
	struct inode *node;
	int min;
	unsigned limit;
	unsigned offset;
	unsigned ro;
	struct loop_device *prev, *next;
} *loop_devices;
mutex_t loop_mutex;
int loop_maj = -1;

struct loop_device *get_loop(int num) 
{
	mutex_on(&loop_mutex);
	struct loop_device *loop = loop_devices;
	while(loop && loop->min != num) loop = loop->next;
	mutex_off(&loop_mutex);
	return loop;
}

void add_loop_device(int num)
{
	struct loop_device *new = (void *)kmalloc(sizeof(struct loop_device));
	new->min = num;
	mutex_on(&loop_mutex);
	struct loop_device *old = loop_devices;
	loop_devices = new;
	new->next = old;
	new->prev=0;
	if(old) old->prev = new;
	mutex_off(&loop_mutex);
}

void remove_loop_device(struct loop_device *loop)
{
	if(!loop) return;
	mutex_on(&loop_mutex);
	if(loop->prev)
		loop->prev->next = loop->next;
	else
		loop_devices = loop->next;
	if(loop->next)
		loop->next->prev = loop->prev;
	mutex_off(&loop_mutex);
	kfree(loop);
}

int loop_rw(int rw, int minor, int block, char *buf)
{
	int ret=0;
	struct loop_device *loop = get_loop(minor);
	if(!loop || !loop->node) return -EINVAL;
	if(loop->ro && rw == WRITE) return -EROFS;
	if((block * 512 + loop->offset + 512) > loop->limit && loop->limit)
		return 0;
	if(rw == READ)
		ret = read_fs(loop->node, block*512 + loop->offset, 512, buf);
	else if(rw == WRITE)
		ret = write_fs(loop->node, block*512 + loop->offset, 512, buf);
	return ret;
}

int loop_up(int num, char *name)
{
	mutex_on(&loop_mutex);
	struct loop_device *loop = get_loop(num);
	if(!loop) {
		mutex_off(&loop_mutex);
		return -EINVAL;
	}
	if(loop->node) {
		mutex_off(&loop_mutex);
		return -EBUSY;
	}
	
	struct inode *i = get_idir(name, 0);
	if(!i) {
		mutex_on(&loop_mutex);
		return -ENOENT;
	}
	mutex_on(&i->lock);
	i->required++;
	mutex_off(&i->lock);
	loop->offset = loop->limit = loop->ro = 0;
	loop->node = i;
	
	mutex_off(&loop_mutex);
	return 0;
}

int loop_down(int num)
{
	mutex_on(&loop_mutex);
	struct loop_device *loop = get_loop(num);
	if(!loop) {
		mutex_off(&loop_mutex);
		return -EINVAL;
	}
	if(!loop->node) {
		mutex_off(&loop_mutex);
		return -EINVAL;
	}
	struct inode *i = loop->node;
	loop->node=0;
	mutex_on(&i->lock);
	i->required--;
	mutex_off(&i->lock);
	iput(i);
	return 0;
}

/*
 * 0: put up a loop device, arg = path.
 * 1: take down loop device
 * 2: apply offset to a loop device; arg = offset
 * 3: apply limit to a loop device, arg = limit. limit of 0 means EOF
 * 4: add this loop device
 * 5: remove this loop device
 * 6: make readonly
 * 7: create new loop device (arg = number)
*/

int ioctl_main(int min, int cmd, int arg)
{
	struct loop_device *loop;
	switch(cmd) {
		case 0:
			return loop_up(min, (char *)arg);
		case 1:
			return loop_down(min);
		case 2:
			loop = get_loop(min);
			if(!loop) return -EINVAL;
			loop->offset = (unsigned)arg;
			break;
		case 3:
			loop = get_loop(min);
			if(!loop) return -EINVAL;
			loop->limit = (unsigned)arg;
			break;
		case 4:
			loop = get_loop(min);
			if(loop) return -EINVAL;
			add_loop_device(min);
			break;
		case 5:
			loop = get_loop(min);
			if(!loop) return -EINVAL;
			if(loop->node) return -EBUSY;
			remove_loop_device(loop);
			break;
		case 6:
			loop = get_loop(min);
			if(!loop) return -EINVAL;
			loop->ro = (unsigned)arg;
		case 7:
			loop = get_loop(arg);
			if(loop) return -EEXIST;
			char tmp[128];
			sprintf(tmp, "loop%d", arg);
			struct inode *i = get_idir(tmp, devfs_root);
			if(i) {
				iput(i);
				return -EEXIST;
			}
			dfs_cn(tmp, S_IFBLK, loop_maj, arg);
			add_loop_device(arg);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int module_install()
{
	loop_maj = set_availablebd(loop_rw, 512, ioctl_main, 0, 0);
	if(loop_maj < 0) return 1;
	device_t *dev = get_device(DT_BLOCK, loop_maj);
	if(dev && dev->ptr) {
		blockdevice_t *bd = dev->ptr;
		bd->cache=0;
	} else {
		unregister_block_device(loop_maj);
		return 1;
	}
	create_mutex(&loop_mutex);
	dfs_cn("loop0", S_IFBLK, loop_maj, 0);
	loop_devices=0;
	add_loop_device(0);
	return 0;
}

int module_exit()
{
	unregister_block_device(loop_maj);
	mutex_on(&loop_mutex);
	while(loop_devices)
		remove_loop_device(loop_devices);
	mutex_off(&loop_mutex);
	destroy_mutex(&loop_mutex);
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
