#include <sea/fs/inode.h>
#include <sea/lib/hash.h>
#include <sea/dm/blockdev.h>
#include <sea/mm/kmalloc.h>
#include <sea/dm/dev.h>

static int block_major;
static int next_minor = 1;
int blockdev_register(struct inode *node, uint64_t partbegin, size_t partlen, size_t blocksize
	ssize_t (*rw)(int dir, struct inode *node, uint64_t start, uint8_t *buffer, size_t count));

{
	struct blockdev *bd = kmalloc(sizeof(struct blockdev));
	bd->partbegin = partbegin;
	bd->parlen = partlen;
	bd->blocksize = blocksize;
	bd->rw = rw;
	mutex_create(&bd->cachelock);
	hash_create(&bd->cache, 0, 4096);

	int num = atomic_fetch_add(&next_minor, 1);
	node->devdata = bd;
	node->phys_dev = GETDEV(block_major, num);
	node->kdev = dm_device_get(block_major);
	return num;
}








int block_select(struct file *file, int rw)
{

}

ssize_t block_rw(int rw, struct file *file, off_t off, uint8_t *buffer, size_t len)
{

}

int block_ioctl(struct file *file, int cmd, long arg)
{

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
}

