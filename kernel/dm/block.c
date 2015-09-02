/* Provides functions for read/write/ctl of block devices */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <sea/lib/cache.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/timing.h>
#undef DT_CHAR

static mutex_t bd_search_lock;

int ioctl_stub(int a, int b, long c)
{
	return -1;
}

blockdevice_t *dm_set_block_device(int maj, int (*f)(int, int, u64, char*), int bs, 
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
	dm_add_device(DT_BLOCK, maj, dev);
	queue_create(&dev->wq, 0);
	kthread_create(&dev->elevator, "[kelevator]", 0, block_elevator_main, dev);
	hash_table_create(&dev->cache, HASH_NOLOCK, HASH_TYPE_CHAIN);
	hash_table_resize(&dev->cache, HASH_RESIZE_MODE_IGNORE,7000); /* TODO: initial value? */
	hash_table_specify_function(&dev->cache, HASH_FUNCTION_DEFAULT);
	mutex_create(&dev->cachelock, MT_NOSCHED);
	return dev;
}

int dm_set_available_block_device(int (*f)(int, int, u64, char*), int bs, 
	int (*c)(int, int, long), int (*m)(int, int, u64, char *, int), int (*s)(int, int))
{
	int i=10; /* first 10 devices are reserved by the system */
	mutex_acquire(&bd_search_lock);
	while(i>0) {
		if(!dm_get_device(DT_BLOCK, i))
		{
			dm_set_block_device(i, f, bs, c, m, s);
			break;
		}
		i++;
	}
	mutex_release(&bd_search_lock);
	if(i < 0)
		return -EINVAL;
	return i;
}

void dm_unregister_block_device(int n)
{
	printk(1, "[dev]: Unregistering block device %d\n", n);
	mutex_acquire(&bd_search_lock);
	device_t *dev = dm_get_device(DT_BLOCK, n);
	if(!dev) {
		mutex_release(&bd_search_lock);
		return;
	}
	void *fr = dev->ptr;
	dm_remove_device(DT_BLOCK, n);
	mutex_release(&bd_search_lock);
	kthread_join(&((blockdevice_t *)(dev->ptr))->elevator, 0);
	mutex_destroy(&((blockdevice_t *)(dev->ptr))->acl);
	kfree(fr);
}

void dm_init_block_devices(void)
{
	mutex_create(&bd_search_lock, 0);
	block_cache_init();
	block_buffer_init();
}

int dm_do_block_rw(int rw, dev_t dev, u64 blk, char *buf, blockdevice_t *bd)
{
	if(dev < 0)
		dev=-dev;
	if(!bd) 
	{
		device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	if(bd->rw)
	{
	//	mutex_acquire(&bd->acl);
		int ret = (bd->rw)(rw, MINOR(dev), blk, buf);
	//	mutex_release(&bd->acl);
		return ret;
	}
	return -EIO;
}

int dm_do_block_rw_multiple(int rw, dev_t dev, u64 blk, char *buf, int count, blockdevice_t *bd)
{
	if(dev < 0)
		dev=-dev;
	if(!bd) 
	{
		device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
		if(!dt)
			return -ENXIO;
		bd = (blockdevice_t *)dt->ptr;
	}
	if(bd->rw_multiple)
	{
		//mutex_acquire(&bd->acl);
		int ret = (bd->rw_multiple)(rw, MINOR(dev), blk, buf, count);
	//	mutex_release(&bd->acl);
		return ret;
	}
	return -EIO;
}

int dm_block_read(dev_t dev, off_t pos, char *buf, size_t count)
{
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	uint64_t start_block = pos / bd->blksz;
	uint64_t end_block = ((pos + count - 1) / bd->blksz);
	size_t num_blocks = (end_block - start_block) + 1;

	struct ioreq *req = ioreq_create(bd, dev, READ, start_block, num_blocks);

	int ret = block_cache_request(req, pos % bd->blksz, count, buf);
	ioreq_put(req);
	return ret;
}

int dm_block_write(dev_t dev, off_t posit, char *buf, size_t count)
{
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return -ENXIO;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	int blk_size = bd->blksz;
	unsigned pos = posit;

	/* If we are offset in a block, we dont wanna overwrite stuff */
	if(pos % blk_size)
	{
		struct llist blist;
		ll_create_lockless(&blist);
		struct ioreq *req = ioreq_create(bd, dev, READ, pos / blk_size, 1);
		if(block_cache_get_bufferlist(&blist, req) != 1) {
			ioreq_put(req);
			return 0;
		}
		ioreq_put(req);
		struct buffer *br = ll_entry(struct buffer *, blist.head);
		/* If count is less than whats remaining, just use count */
		int write = (blk_size-(pos % blk_size));
		if(count < (unsigned)write)
			write=count;
		memcpy(br->data+(pos % blk_size), buf, write);
		or_atomic(&br->flags, BUFFER_DIRTY);
		buffer_put(br);
		buf += write;
		count -= write;
		pos += write;
	}
	while(count >= (unsigned int)blk_size)
	{
		assert((pos & ~(blk_size - 1)) == pos);

		struct buffer *entry = dm_block_cache_get(bd, pos / blk_size);
		if(!entry) {
			entry = buffer_create(bd, dev, pos / blk_size, BUFFER_DIRTY, buf);
			memcpy(entry->data, buf, blk_size);
			dm_block_cache_insert(bd, pos/blk_size, entry, BLOCK_CACHE_OVERWRITE);
		} else {
			memcpy(entry->data, buf, blk_size);
			or_atomic(&entry->flags, BUFFER_DIRTY);
		}
		buffer_put(entry);
		count -= blk_size;
		pos += blk_size;
		buf += blk_size;
	}
	/* Anything left over? */
	if(count > 0)
	{
		struct llist blist;
		ll_create_lockless(&blist);
		struct ioreq *req = ioreq_create(bd, dev, READ, pos/blk_size, 1);
		if(block_cache_get_bufferlist(&blist, req) != 1) {
			ioreq_put(req);
			return 0;
		}
		ioreq_put(req);
		struct buffer *br = ll_entry(struct buffer *, blist.head);
		memcpy(br->data, buf, count);
		or_atomic(&br->flags, BUFFER_DIRTY);
		buffer_put(br);
		pos+=count;
	}
	return pos-posit;
}

/* General functions */
int dm_block_device_rw(int mode, dev_t dev, off_t off, char *buf, size_t len)
{
	if(mode == READ)
		return dm_block_read(dev, off, buf, len);
	if(mode == WRITE)
		return dm_block_write(dev, off, buf, len);
	return -EINVAL;
}

/* Reserved commands:
 * -1: Sync any data in device buffer
 */
int dm_block_ioctl(dev_t dev, int cmd, long arg)
{
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
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

int dm_block_device_select(dev_t dev, int rw)
{
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
	if(!dt)
		return 1;
	blockdevice_t *bd = (blockdevice_t *)dt->ptr;
	if(bd->select)
		return bd->select(MINOR(dev), rw);
	return 1;
}

int dm_blockdev_select(struct inode *in, int rw)
{
	return dm_block_device_select(in->phys_dev, rw);
}

void dm_send_sync_block(void)
{
	int i=0;
	while(i>=0) {
		device_t *d = dm_get_enumerated_device(DT_BLOCK, i);
		if(!d) break;
		assert(d->ptr);
		blockdevice_t *cd = (blockdevice_t *)d->ptr;
		if(cd->ioctl)
			cd->ioctl(0, -1, 0);
		i++;
		if(tm_thread_got_signal(current_thread))
			return;
	}
}
