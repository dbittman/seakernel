#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/sys/stat.h>
#include <sea/dm/dev.h>
#include <sea/lib/cache.h>
#include <sea/dm/block.h>
#include <sea/ll.h>
#include <sea/fs/devfs.h>
struct loop_device {
	struct inode *node;
	int min;
	unsigned limit;
	unsigned offset;
	unsigned ro;
};

struct llist *loops;
int loop_maj = -1;

struct loop_device *get_loop(int num) 
{
	rwlock_acquire(&loops->rwl, RWL_READER);
	struct llistnode *cur;
	struct loop_device *ld;
	ll_for_each_entry(loops, cur, struct loop_device *, ld)
	{
		if(ld->min == num)
		{
			rwlock_release(&loops->rwl, RWL_READER);
			return ld;
		}
	}
	rwlock_release(&loops->rwl, RWL_READER);
	return 0;
}

void add_loop_device(int num)
{
	struct loop_device *new = (void *)kmalloc(sizeof(struct loop_device));
	new->min = num;
	ll_insert(loops, new);
}

void remove_loop_device(struct loop_device *loop)
{
	if(!loop) return;
	ll_remove_entry(loops, loop);
	kfree(loop);
}

int loop_rw(int rw, int minor, u64 block, char *buf)
{
	int ret=0;
	struct loop_device *loop = get_loop(minor);
	if(!loop || !loop->node) return -EINVAL;
	if(loop->ro && rw == WRITE) return -EROFS;
	if((block * 512 + loop->offset + 512) > loop->limit && loop->limit)
		return 0;
	if(rw == READ)
		ret = vfs_read_inode(loop->node, block*512 + loop->offset, 512, buf);
	else if(rw == WRITE)
		ret = vfs_write_inode(loop->node, block*512 + loop->offset, 512, buf);
	return ret;
}

int loop_up(int num, char *name)
{
	struct loop_device *loop = get_loop(num);
	if(!loop) 
		return -EINVAL;
	if(loop->node) 
		return -EBUSY;
	
	struct inode *i = vfs_get_idir(name, 0);
	if(!i)
		return -ENOENT;
	loop->offset = loop->limit = loop->ro = 0;
	loop->node = i;
	
	return 0;
}

int loop_down(int num)
{
	struct loop_device *loop = get_loop(num);
	if(!loop)
		return -EINVAL;
	if(!loop->node)
		return -EINVAL;
	struct inode *i = loop->node;
	loop->node=0;
	vfs_iput(i);
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

int ioctl_main(int min, int cmd, long arg)
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
			snprintf(tmp, 128, "loop%d", arg);
			struct inode *i = vfs_get_idir(tmp, devfs_root);
			if(i) {
				vfs_iput(i);
				return -EEXIST;
			}
			devfs_add(devfs_root, tmp, S_IFBLK, loop_maj, arg);
			add_loop_device(arg);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int module_install()
{
	loop_maj = dm_set_available_block_device(loop_rw, 512, ioctl_main, 0, 0);
	if(loop_maj < 0) return EINVAL;
	device_t *dev = dm_get_device(DT_BLOCK, loop_maj);
	if(dev && dev->ptr) {
		blockdevice_t *bd = dev->ptr;
		bd->cache=0;
	} else {
		dm_unregister_block_device(loop_maj);
		return EINVAL;
	}
	loops = ll_create(0);
	devfs_add(devfs_root, "loop0", S_IFBLK, loop_maj, 0);
	add_loop_device(0);
	return 0;
}

int module_tm_exit()
{
	dm_unregister_block_device(loop_maj);
	if(ll_is_active(loops))
	{
		struct llistnode *cur, *next;
		struct loop_device *ld;
		ll_for_each_entry_safe(loops, cur, next, struct loop_device *, ld)
		{
			ll_remove(loops, cur);
			kfree(ld);
		}
	}
	ll_destroy(loops);
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
