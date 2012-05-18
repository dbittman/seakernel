/* Provides access layer to the kernel cache for block devices (write-through block cache. Speeds up writing */
#include <kernel.h>
#include <dev.h>
#include <block.h>
#include <cache.h>
cache_t *blk_cache=0;

int block_cache_sync(struct ce_t *c)
{
	int dev = c->id;
	int blk = c->key;
	if(c->dirty)
		do_block_rw(WRITE, dev, blk, c->data, 0);
	return 1;
}

void block_cache_init()
{
	blk_cache = get_empty_cache(block_cache_sync, "block");
}

int disconnect_block_cache(int dev)
{
	return destroy_all_id(blk_cache, dev);
}

int cache_block(int dev, unsigned blk, int sz, char *buf)
{
	return do_cache_object(blk_cache, dev < 0 ? -dev : dev, blk, sz, buf, dev < 0 ? 0 : 1);
}

int get_block_cache(int dev, int blk, char *buf)
{
	struct ce_t *c = find_cache_element(blk_cache, dev, blk);
	if(!c)
		return 0;
	memcpy(buf, c->data, c->length);
	return 1;
}

int write_block_cache(int dev, int blk)
{
	struct ce_t *c = find_cache_element(blk_cache, dev, blk);
	block_cache_sync(c);
	return 1;
}

int proc_read_bcache(char *buf, int off, int len)
{
	return 0;
}
