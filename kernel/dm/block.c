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
#include <sea/tm/timing.h>
#undef DT_CHAR

static mutex_t bd_search_lock;

int ioctl_stub(int a, int b, long c)
{
	return -1;
}

int __elevator_main(struct kthread *kt, void *arg)
{
	blockdevice_t *dev = arg;
	const int max = 8;
	char *buf = kmalloc(dev->blksz * max);
	while(!kthread_is_joining(kt)) {
		struct queue_item *qi;
		if((qi = queue_dequeue_item(&dev->wq))) {
			struct ioreq *req = qi->ent;
			size_t count = req->count;
			size_t block = req->block;
			while(count) {
				int this = 8;
				if(this > count)
					this = count;
				assert(req->direction == READ);
				int ret = dm_do_block_rw_multiple(req->direction, req->dev, block, buf, this, dev);
				if(ret == dev->blksz * this) {
					for(int i=0;i<this;i++) {
						dm_cache_block(-req->dev, block+i, dev->blksz, buf+i*dev->blksz);
					}
				} else {
					req->flags |= IOREQ_FAILED;
				}
				block+=this;
				count-=this;
			}
			req->flags |= IOREQ_COMPLETE;
			tm_blocklist_wakeall(&req->blocklist);
			if(sub_atomic(&req->refs, 1) == 0)
				kfree(req);
		} else {
			tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
			tm_schedule();
		}
	}
	return 0;
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
	dev->cache = BCACHE_WRITE | (CONFIG_BLOCK_READ_CACHE ? BCACHE_READ : 0);
	dm_add_device(DT_BLOCK, maj, dev);
	queue_create(&dev->wq, 0);
	kthread_create(&dev->elevator, "[kelevator]", 0, __elevator_main, dev);
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
#if CONFIG_BLOCK_CACHE
	dm_block_cache_init();
#endif
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

void __add_request(struct ioreq *req)
{
	add_atomic(&req->refs, 1);
	queue_enqueue_item(&req->bd->wq, &req->qi, req);
	tm_thread_set_state(req->bd->elevator.thread, THREADSTATE_RUNNING);
}

int block_cache_request(struct ioreq *req, off_t offset, size_t bytecount)
{
	int ret;
	size_t count = req->count;
	size_t block = req->block;
	size_t position = 0;
	char *buffer = req->buffer;
	while(count) {
		char tmp[req->bd->blksz];
		ret = dm_get_block_cache(req->dev, block, tmp);
		if(!ret) {
			/* TODO: cleanup patterns like this */
			cpu_disable_preemption();
			tm_thread_add_to_blocklist(current_thread, &req->blocklist);
			mutex_acquire(&current_thread->block_mutex);
			__add_request(req);
			if(current_thread->blocklist)
				tm_thread_set_state(current_thread, THREADSTATE_UNINTERRUPTIBLE);
			mutex_release(&current_thread->block_mutex);
			cpu_enable_preemption();
			tm_schedule();
			assert(req->flags & IOREQ_COMPLETE);
			if(req->flags & IOREQ_FAILED) {
				return position;
			}
		} else {
			size_t copy_amount = req->bd->blksz - offset;
			if(copy_amount > bytecount)
				copy_amount = bytecount;
			memcpy(buffer + position, tmp + offset, copy_amount);
			block++;
			count--;
			bytecount -= copy_amount;
			position += copy_amount;
			offset = 0;
			if(!bytecount)
				assert(!count);
		}
	}
	return position;
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

	struct ioreq *req = kmalloc(sizeof(*req));
	req->block = start_block;
	req->count = num_blocks;
	req->direction = READ;
	req->bd = bd;
	req->dev = dev;
	req->flags = 0;
	req->buffer = buf;
	req->refs = 1;
	ll_create(&req->blocklist);

	int ret = block_cache_request(req, pos % bd->blksz, count);
	if(sub_atomic(&req->refs, 1) == 0)
		kfree(req);
	return ret;
}

int dm_block_write(dev_t dev, off_t posit, char *buf, size_t count)
{
	device_t *dt = dm_get_device(DT_BLOCK, MAJOR(dev));
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
		if(dm_block_read(dev, pos & ~(blk_size - 1), buffer, blk_size) != blk_size) {
			return 0;
		}
		/* If count is less than whats remaining, just use count */
		int write = (blk_size-(pos % blk_size));
		if(count < (unsigned)write)
			write=count;
		memcpy(buffer+(pos % blk_size), buf, write);
		dm_cache_block(dev, pos/blk_size, bd->blksz, buffer);
		buf += write;
		count -= write;
		pos += write;
	}
	while(count >= (unsigned int)blk_size)
	{
		assert((pos & ~(blk_size - 1)) == pos);
		dm_cache_block(dev, pos/blk_size, bd->blksz, buf);
		count -= blk_size;
		pos += blk_size;
		buf += blk_size;
	}
	/* Anything left over? */
	if(count > 0)
	{
		if(dm_block_read(dev, pos & ~(blk_size - 1), buffer, blk_size) != blk_size) {
			return pos-posit;
		}
		memcpy(buffer, buf, count);
		dm_cache_block(dev, pos/blk_size, bd->blksz, buffer);
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
