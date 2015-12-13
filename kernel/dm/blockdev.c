#include <sea/fs/inode.h>
#include <sea/lib/hash.h>
#include <sea/dm/blockdev.h>
#include <sea/mm/kmalloc.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <sea/errno.h>
#include <sea/loader/symbol.h>
#include <sea/fs/kerfs.h>
static int block_major;
static int next_minor = 1;
int blockdev_register(struct inode *node, struct blockctl *ctl)
{
	struct blockdev *bd = kmalloc(sizeof(struct blockdev));
	mutex_create(&ctl->cachelock, 0);
	hash_create(&ctl->cache, 0, 0x4000);
	mpscq_create(&ctl->queue, 1000);
	bd->ctl = ctl;

	int num = atomic_fetch_add(&next_minor, 1);
	node->devdata = bd;
	node->phys_dev = GETDEV(block_major, num);
	node->kdev = dm_device_get(block_major);
	
	kthread_create(&ctl->elevator, "[kelevator]", 0, block_elevator_main, node);
	char name[64];
	snprintf(name, 64, "/dev/bcache-%d", num);
	kerfs_register_parameter(name, ctl, 0, 0, kerfs_block_cache_report);
	return num;
}

void blockdev_register_partition(struct inode *master, struct inode *part, uint64_t partbegin, uint64_t partlen)
{
	struct blockdev *mbd = master->devdata;
	struct blockdev *pbd = kmalloc(sizeof(struct blockdev));
	pbd->ctl = mbd->ctl;
	pbd->partbegin = partbegin;
	pbd->partlen = partlen;
	part->devdata = pbd;
	part->phys_dev = master->phys_dev;
	part->kdev = dm_device_get(block_major);
}

static ssize_t __block_read(struct file *file, off_t pos, uint8_t *buf, size_t count)
{
	struct blockdev *bd = file->inode->devdata;
	uint64_t start_block = pos / bd->ctl->blocksize;
	uint64_t end_block = ((pos + count - 1) / bd->ctl->blocksize);
	size_t num_blocks = (end_block - start_block) + 1;

	struct ioreq *req = ioreq_create(bd, READ, start_block, num_blocks);

	int ret = block_cache_request(req, pos % bd->ctl->blocksize, count, buf);
	ioreq_put(req);
	return ret;
}

static ssize_t __block_write(struct file *file, off_t posit, uint8_t *buf, size_t count)
{
	struct blockdev *bd = file->inode->devdata;
	int blk_size = bd->ctl->blocksize;
	unsigned pos = posit;

	/* If we are offset in a block, we dont wanna overwrite stuff */
	if(pos % blk_size)
	{
		struct ioreq *req = ioreq_create(bd, READ, pos / blk_size, 1);
		struct buffer *br = block_cache_get_first_buffer(req);
		ioreq_put(req);
		if(!br)
			return 0;
		/* If count is less than whats remaining, just use count */
		int write = (blk_size-(pos % blk_size));
		if(count < (unsigned)write)
			write=count;
		memcpy(br->data+(pos % blk_size), buf, write);
		atomic_fetch_or(&br->flags, BUFFER_DIRTY);
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
			entry = buffer_create(bd, pos / blk_size, BUFFER_DIRTY, buf);
			memcpy(entry->data, buf, blk_size);
			dm_block_cache_insert(bd, pos/blk_size, entry, BLOCK_CACHE_OVERWRITE);
		} else {
			memcpy(entry->data, buf, blk_size);
			atomic_fetch_or(&entry->flags, BUFFER_DIRTY);
		}
		buffer_put(entry);
		count -= blk_size;
		pos += blk_size;
		buf += blk_size;
	}
	/* Anything left over? */
	if(count > 0)
	{
		struct ioreq *req = ioreq_create(bd, READ, pos/blk_size, 1);
		struct buffer *br = block_cache_get_first_buffer(req);
		ioreq_put(req);
		if(!br)
			return 0;
		memcpy(br->data, buf, count);
		atomic_fetch_or(&br->flags, BUFFER_DIRTY);
		buffer_put(br);
		pos+=count;
	}
	return pos-posit;
}

int block_select(struct file *file, int rw)
{
	struct blockdev *bd = file->inode->devdata;
	assert(bd);
	if(bd->ctl->select)
		return bd->ctl->select(file->inode, rw);
	return 1;
}

ssize_t block_rw(int rw, struct file *file, off_t off, uint8_t *buffer, size_t len)
{
	if(rw == READ)
		return __block_read(file, off, buffer, len);
	else if(rw == WRITE)
		return __block_write(file, off, buffer, len);
	return -EIO;
}

int block_ioctl(struct file *file, int cmd, long arg)
{
	struct blockdev *bd = file->inode->devdata;
	assert(bd);
	if(bd->ctl->ioctl)
		return bd->ctl->ioctl(file->inode, cmd, arg);
	return -EINVAL;
}

static struct kdevice __kdev_block = {
	.select = block_select,
	.rw = block_rw,
	.ioctl = block_ioctl,
	.open = 0, .close = 0, .create = 0, .destroy = 0,
	.name = "block",
};

void blockdev_init(void)
{
	block_major = dm_device_register(&__kdev_block);
	block_cache_init();
	block_buffer_init();
	
	loader_add_kernel_symbol(blockdev_register);
	loader_add_kernel_symbol(blockdev_register_partition);
}

