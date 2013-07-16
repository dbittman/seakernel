/* Provides functions for read/write/ctl of block devices */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <dev.h>
#include <block.h>
#include <cache.h>
#undef DT_CHAR
mutex_t bd_search_lock;
int ioctl_stub(int a, int b, long c)
{
	return -1;
}

blockdevice_t *set_blockdevice(int maj, int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int))
{
	printk(1, "[dev]: Setting block device %d, bs=%d (%x, %x)\n", maj, bs, f, c);
	blockdevice_t *dev = (blockdevice_t *)kmalloc(sizeof(blockdevice_t));
	dev->rw = f;
	dev->blksz=bs;
	dev->ioctl=c;
	dev->rw_multiple=m;
	dev->select = s;
	mutex_create(&dev->acl, 0);
	if(!c)
		dev->ioctl=ioctl_stub;
	dev->cache = BCACHE_WRITE | (CACHE_READ ? BCACHE_READ : 0);
	add_device(DT_BLOCK, maj, dev);
	return dev;
}

int set_availablebd(int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int))
{
	int i=10; /* first 10 devices are reserved by the system */
	mutex_acquire(&bd_search_lock);
	while(i>0) {
		if(!get_device(DT_BLOCK, i))
		{
			set_blockdevice(i, f, bs, c, m, s);
			break;
		}
		i++;
	}
	mutex_release(&bd_search_lock);
	if(i < 0)
		return -EINVAL;
	return i;
}

void unregister_block_device(int n)
{
	printk(1, "[dev]: Unregistering block device %d\n", n);
	int i;
	for(i=0;i<256;i++)
		disconnect_block_cache(GETDEV(n, i));
	mutex_acquire(&bd_search_lock);
	device_t *dev = get_device(DT_BLOCK, n);
	if(!dev) {
		mutex_release(&bd_search_lock);
		return;
	}
	void *fr = dev->ptr;
	remove_device(DT_BLOCK, n);
	mutex_release(&bd_search_lock);
	mutex_destroy(&((blockdevice_t *)(dev->ptr))->acl);
	kfree(fr);
}

void init_block_devs()
{
	mutex_create(&bd_search_lock, 0);
	block_cache_init();
}

int do_block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd)
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
	if(bd->rw)
	{
		mutex_acquire(&bd->acl);
		int ret = (bd->rw)(rw, MINOR(dev), blk, buf);
		mutex_release(&bd->acl);
		return ret;
	}
	return -EIO;
}

int block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd)
{
	if(!bd) 
	{
		device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	int ret=0;
	if(rw == READ)
	{
#if USE_CACHE
		if(bd->cache) ret = get_block_cache(dev, blk, buf);
		if(ret)
			return bd->blksz;
#endif
		ret = do_block_rw(rw, dev, blk, buf, bd);
#if USE_CACHE
		/* -dev signals that this is a read cache - 
		 * meaning it is not 'dirty' to start with */
		if(ret == bd->blksz && (bd->cache & BCACHE_READ)) 
			cache_block(-dev, blk, bd->blksz, buf);
#endif
	} else if(rw == WRITE)
	{
#if USE_CACHE
		if(bd->cache) 
		{
			if(!cache_block(dev, blk, bd->blksz, buf))
				return bd->blksz;
		}
#endif
		ret = do_block_rw(rw, dev, blk, buf, bd);
	}
	return ret;
}

/* reads many blocks and caches them */
unsigned do_block_read_multiple(blockdevice_t *bd, dev_t dev, u64 start, 
	unsigned num, char *buf)
{
	unsigned count=0;
	/* if we don't support reading multiple blocks, then just loop through
	 * and read them individually. block_rw will cache them */
	if(!bd->rw_multiple) {
		while(num--) {
			if(block_rw(READ, dev, start+count, buf+count*bd->blksz, bd) != bd->blksz)
				return count;
			count++;
		}
		return count;
	}
	num = bd->rw_multiple(READ, MINOR(dev), start, buf, num) / bd->blksz;
#if USE_CACHE
	if(bd->cache & BCACHE_READ) {
		while(count < num) {
			cache_block(-dev, start+count, bd->blksz, buf + count*bd->blksz);
			count++;
		}
	} else
		count=num;
#else
	count = num;
#endif
	return count;
}

unsigned block_read_multiple(blockdevice_t *bd, int dev, u64 start, 
	unsigned num, char *buf)
{
	unsigned count=0;
	int ret;
#if USE_CACHE
	if(bd->cache & BCACHE_READ)  {
		/* if we're gonna cache them, then we need to do some work. We try
		 * to read any block that is in the cache from the cache, and only
		 * ask the block device to read if we have to. So we loop though
		 * to find where we need to start reading from the block device, and
		 * then also loop through to figure out how many block we need to read
		 * before we can read from the cache again. This is a simple implementation, 
		 * and it could be more complicated. This gives the minimum reads from
		 * the block device and reads in the largest chunks possible */
		while(count<num) {
			ret = get_block_cache(dev, start+count, buf + count*bd->blksz);
			if(!ret) {
				unsigned x = count+1;
				while(x < num && !get_block_cache(dev, start+x, buf + x*bd->blksz))
					x++;
				unsigned r;
				if(x-count == 1)
					r = block_rw(READ, dev, start+count, buf+count*bd->blksz, bd)/bd->blksz;
				else
					r = do_block_read_multiple(bd, dev, start+count, x-count, buf + count*bd->blksz);
				if(r != x-count)
					return count+r;
				count = x;
				if(x < num)
					count++;
			} else
				count++;
		}
	} else
		count=do_block_read_multiple(bd, dev, start, num, buf);
#else
	count=do_block_read_multiple(bd, dev, start, num, buf);
#endif
	return count;
}

int block_read(dev_t dev, off_t posit, char *buf, size_t c)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	unsigned blk_size = bd->blksz;
	unsigned count=0;
	unsigned pos=posit;
	unsigned offset=0;
	unsigned end = pos + c;
	unsigned i=0;
	int ret;
	char tmp[blk_size];
	/* read in the first (possibly) partial block */
	offset = pos % blk_size;
	if(offset) {
		ret = block_rw(READ, dev, pos/blk_size, tmp, bd);
		if(ret != (int)blk_size)
			return count;
		count = blk_size - offset;
		if(count > c) count = c;
		memcpy(buf, tmp + offset, c);
		pos += count;
	}
	if(count >= c) return count;
	/* read in the bulk full blocks */
	if((c-count) >= blk_size) {
		i = (c-count)/blk_size;
		unsigned int r = block_read_multiple(bd, dev, pos / blk_size, i, buf + count);
		if(r != i)
			return count + r*blk_size;
		pos += i * blk_size;
		count += i * blk_size;
	}
	if(count >= c) return count;
	/* read in the remainder */
	if(end % blk_size && pos < end) {
		ret = block_rw(READ, dev, pos/blk_size, tmp, bd);
		if(ret != (int)blk_size)
			return count;
		memcpy(buf+count, tmp, end % blk_size);
		count += end % blk_size;
	}
	return count;
}

int block_write(dev_t dev, off_t posit, char *buf, size_t count)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	if(!count) return 0;
	int blk_size = bd->blksz;
	unsigned pos = posit;
	char buffer[blk_size];
	/* If we are offset in a block, we dont wanna overwrite stuff */
	if(pos % blk_size)
	{
		if(block_rw(READ, dev, pos/blk_size, buffer, bd) != blk_size)
			return 0;
		/* If count is less than whats remaining, just use count */
		int write = (blk_size-(pos % blk_size));
		if(count < (unsigned)write)
			write=count;
		memcpy(buffer+(pos % blk_size), buf, write);
		if(block_rw(WRITE, dev, pos/blk_size, buffer, bd) != blk_size)
			return 0;
		buf += write;
		count -= write;
		pos += write;
	}
	while(count >= (unsigned int)blk_size)
	{
		if(block_rw(WRITE, dev, pos/blk_size, buf, bd) != blk_size)
			return (pos-posit);
		count -= blk_size;
		pos += blk_size;
		buf += blk_size;
	}
	/* Anything left over? */
	if(count > 0)
	{
		if(block_rw(READ, dev, pos/blk_size, buffer, bd) != blk_size)
			return pos-posit;
		memcpy(buffer, buf, count);
		block_rw(WRITE, dev, pos/blk_size, buffer, bd);
		pos+=count;
	}
	return pos-posit;
}

/* General functions */
int block_device_rw(int mode, dev_t dev, off_t off, char *buf, size_t len)
{
	if(mode == READ)
		return block_read(dev, off, buf, len);
	if(mode == WRITE)
		return block_write(dev, off, buf, len);
	return -EINVAL;
}

/* Reserved commands:
 * -1: Sync any data in device buffer
 */
int block_ioctl(dev_t dev, int cmd, int arg)
{
	device_t *dt = get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
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
		if(got_signal(current_task))
			return;
	}
}
