/* Provides functions for read/write/ctl of block devices */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <dev.h>
#include <block.h>
#include <cache.h>
#undef DT_CHAR
void block_cache_init();
int ioctl_stub(int a, int b, int c)
{
	return -1;
}

blockdevice_t *set_blockdevice(int maj, int (*f)(int, int, int, char*), int bs, 
	int (*c)(int, int, int), int (*m)(int, int, int, char *, int), int (*s)(int, int))
{
	printk(1, "[dev]: Setting block device %d, bs=%d (%x, %x)\n", maj, bs, f, c);
	blockdevice_t *dev = (blockdevice_t *)kmalloc(sizeof(blockdevice_t));
	dev->func = f;
	dev->blksz=bs;
	dev->ioctl=c;
	dev->func_m=m;
	dev->select = s;
	create_mutex(&dev->acl);
	if(!c)
		dev->ioctl=ioctl_stub;
	dev->cache = BCACHE_WRITE | (CACHE_READ ? BCACHE_READ : 0);
	add_device(DT_BLOCK, maj, dev);
	return dev;
}

int set_availablebd(int (*f)(int, int, int, char*), int bs, int (*c)(int, int, int), int (*m)(int, int, int, char *, int), int (*s)(int, int))
{
	int i=10;
	while(i>0) {
		if(!get_device(DT_BLOCK, i))
		{
			set_blockdevice(i, f, bs, c, m, s);
			break;
		}
		i++;
	}
	if(i < 0)
		return -EINVAL;
	return i;
}

void unregister_block_device(int n)
{
	printk(1, "[dev]: Unregistering block device %d\n", n);
	int i;
	for(i=0;i<256;i++)
	{
		disconnect_block_cache(GETDEV(n, i));
	}
	device_t *dev = get_device(DT_BLOCK, n);
	if(!dev) return;
	void *fr = dev->ptr;
	remove_device(DT_BLOCK, n);
	destroy_mutex(&((blockdevice_t *)(dev->ptr))->acl);
	kfree(fr);
}

void init_block_devs()
{
	block_cache_init();
}

int do_block_rw(int rw, int dev, int blk, char *buf, blockdevice_t *bd)
{
	if(dev < 0)
		dev=-dev;
	if(!bd) 
	{
		device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	if(bd->func)
	{
		__super_sti();
		int ret = (bd->func)(rw, MINOR(dev), blk, buf);
		return ret;
	}
	return -EIO;
}

int do_block_rw_multiple(int rw, int dev, int blk, char *buf, blockdevice_t *bd, int count)
{
	if(dev < 0)
		dev=-dev;
	if(!bd) 
	{
		device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	if(bd->func_m)
	{
		__super_sti();
		int ret = (bd->func_m)(rw, MINOR(dev), blk, buf, count);
		return ret;
	} else
	{
		panic(PANIC_NOSYNC, "Tried to write multiple blocks to driver that doesn't support that!");
	}
	return -EIO;
}

int block_rw_multiple(int rw, int dev, int blk, char *buf, blockdevice_t *bd, int count)
{
	if(!bd) 
	{
		device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	int ret=0;
	int byt=0;
	unsigned char u_cache = (bd->ioctl)(MINOR(dev), -5, 0);
	if(u_cache == (unsigned char)-1)
		u_cache = bd->cache;
	char buf__[bd->blksz];
	if(rw == READ)
	{
		while(count) {
			if(u_cache)
				ret = get_block_cache(dev, blk, buf);
			int x=1;
			if(!ret || !u_cache)
			{
				ret = do_block_rw_multiple(rw, dev, blk, buf, bd, x);
				if(u_cache & BCACHE_READ && ret == bd->blksz) 
				{
					int i;
					for(i=0;i<x;i++)
						cache_block(-dev, blk+i, bd->blksz, buf+bd->blksz*i);
				}
			}
			byt+=bd->blksz * x;
			buf += bd->blksz * x;
			blk+=x;
			count -= x;
		}
	} else if(rw == WRITE)
	{
		int x=0;
		while(u_cache && x < count) {
			ret += cache_block(dev, blk+x, bd->blksz, buf + bd->blksz*(x));
			x++;
		}
		if(!(u_cache & BCACHE_WRITE) || ret)
			ret = do_block_rw_multiple(rw, dev, blk, buf, bd, count);
		byt+=bd->blksz * count;
	}
	return byt;
}

int block_rw(int rw, int dev, int blk, char *buf, blockdevice_t *bd)
{
	if(!bd) 
	{
		device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	int ret=0;
	mutex_on(&bd->acl);
	unsigned char u_cache = (bd->ioctl)(MINOR(dev), -5, 0);
	if(u_cache == (unsigned char)-1)
		u_cache = bd->cache;
	if(rw == READ)
	{
#if USE_CACHE
		if(u_cache) {
			ret = get_block_cache(dev, blk, buf);
			if(ret) {
				mutex_off(&bd->acl);
				return bd->blksz;
			}
		}
#endif
		ret = do_block_rw(rw, dev, blk, buf, bd);
#if USE_CACHE
		/* -dev signals that this is a read cache - meaning it is not 'dirty' to start with */
		if(u_cache & BCACHE_READ && ret == bd->blksz) 
			cache_block(-dev, blk, bd->blksz, buf);
#endif
	} else if(rw == WRITE)
	{
#if USE_CACHE
		if(u_cache)
			ret = cache_block(dev, blk, bd->blksz, buf);
		if(!(u_cache & BCACHE_WRITE) || ret)
#endif
			ret = do_block_rw(rw, dev, blk, buf, bd);
	}
	mutex_off(&bd->acl);
	return ret;
}

long long block_read(int dev, unsigned long long posit, char *buf, unsigned int c)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	int blk_size = bd->blksz;
	int pos = posit;
	char buffer[blk_size];
	int count, done=0;
	while(c > 0 && !done) {
		if(block_rw(READ, dev, pos/blk_size, buffer, bd) == -1)
			return (pos - posit);
		count = blk_size - (pos % blk_size);
		if(c < (unsigned int)blk_size) {
			count = c;
			done=1;
		}
		memcpy(buf, buffer+(pos % blk_size), count);
		buf+=count;
		pos += count;
		c -= count;
	}
	return pos-posit;
}

/* Hmm...unfortunatly, block writing is a bit trickier...Cause we don't wanna overwrite
   other data. If pos is @ the beginging of a block and the count ends at the end of a block, '
   then it'd be easy, but its just not so all the time! :(. Still, it's not that bad.
*/
 long long  block_write(int dev, unsigned long long posit, char *buf, unsigned int count)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	if(!count) return 0;
	int blk_size = bd->blksz;
	int pos = posit;
	char buffer[blk_size];
	/* If we are offset in a block, we dont wanna overwrite stuff */
	if(pos % blk_size)
	{
		block_rw(READ, dev, pos/blk_size, buffer, bd);
		/* If count is less than whats remaining, just use count */
		int write = (blk_size-(pos % blk_size));
		if(count < (unsigned)write)
			write=count;
		memcpy(buffer+(pos % blk_size), buf, write);
		block_rw(WRITE, dev, pos/blk_size, buffer, bd);
		buf += write;
		count -= write;
		pos += write;
	}
	int g_ = count - (count % 512);
	g_ /= blk_size;
	if(g_)
		block_rw_multiple(WRITE, dev, pos/blk_size, buf, bd, g_);
	count -= g_*blk_size;
	pos += g_*blk_size;
	buf += g_*blk_size;
	/* Anything left over? */
	if(count > 0)
	{
		block_rw(READ, dev, pos/blk_size, buffer, bd);
		memcpy(buffer, buf, count);
		block_rw(WRITE, dev, pos/blk_size, buffer, bd);
		pos+=count;
	}
	return pos-posit;
}

/* General functions */
int block_device_rw(int mode, int dev, int off, char *buf, int len)
{
	if(mode == READ)
		return block_read(dev, off, buf, len);
	if(mode == WRITE)
		return block_write(dev, off, buf, len);
	return -EINVAL;
}

/* Reserved commands:
 * -1: Sync any data in device buffer
 * -2: Return current cache settings
 * -3: Set new cache settings. This will only delete the cache if no-caching is selected.
 * -4: Sync and destroy cache
 * [-5: Asks driver what the caching is on a specific device
 * [-6: Tells driver to change caching info on a specfic device
 * [-7: Ask for size of device (specific to minor)
 */
int block_ioctl(int dev, int cmd, int arg)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	/* Reads the flags */
	unsigned char u_cache = (bd->ioctl)(MINOR(dev), -5, 0);
	char glob=0;
	if(u_cache == (unsigned char)-1)
		u_cache = bd->cache, glob=1;
	if(cmd == -2)
	{
		return u_cache;
	}
	if(cmd == -3) {
		if((current_task->uid == 0)) {
			if(!(arg & BCACHE_WRITE) && (!arg & BCACHE_READ) && u_cache)
			{
				printk(0, "[BCACHE]: cache disabled on %x.", dev);
				if(u_cache & BCACHE_WRITE) {
					printk(0, " Syncing...");
					sync_block_device(dev);
				}
				printk(0, "\n");
				task_critical();
				int x = disconnect_block_cache_1(dev);
				task_uncritical();
				if(x >= 0)
					disconnect_block_cache_2(x);
			}
			if(glob)
				return (bd->cache=arg);
			else
			{
				(bd->ioctl)(MINOR(dev), -6, arg);
				return arg;
			}
		}
		else
			return -1;
	}
	if(cmd == -4)
	{
		if((current_task->uid == 0)) {
			int ret=0;
			printk(0, "[BCACHE]: cache disengaging on %x.", dev);
			if(u_cache & BCACHE_WRITE) {
				printk(0, " Syncing...");
				sync_block_device(dev);
				ret++;
			}
			printk(0, "\n");
			task_critical();
			int x = disconnect_block_cache_1(dev);
			task_uncritical();
			if(x >= 0)
				disconnect_block_cache_2(x);
			return ret;
		}
		return -1;
	}
	if(bd->ioctl)
	{
		int ret = (bd->ioctl)(MINOR(dev), cmd, arg);
		return ret;
	} else
		return 0;
}

int blockdev_select(struct inode *in, int rw)
{
	int dev = in->dev;
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return 1;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	if(bd->select)
		return bd->select(MINOR(dev), rw);
	return 1;
}

void send_sync_block()
{
	int i=0;
	while(i>=0) {
		device_t *d = get_n_device(DT_BLOCK, i);
		if(!d) break;
		assert(d->ptr);
		blockdevice_t *cd = (blockdevice_t *)d->ptr;
		if(cd->ioctl)
			cd->ioctl(0, -1, 0);
		i++;
	}
}
