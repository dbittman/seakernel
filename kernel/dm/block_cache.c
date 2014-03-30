/* Provides access layer to the kernel cache for block devices 
 * (write-through block cache. Speeds up writing */
#include <sea/config.h>
#if CONFIG_BLOCK_CACHE
#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <sea/lib/cache.h>
#include <sea/loader/symbol.h>

static cache_t *blk_cache=0;

int dm_block_cache_sync(struct ce_t *c)
{
	u64 dev = c->id;
	u64 blk = c->key;
	if(c->dirty)
		dm_do_block_rw(WRITE, dev, blk, c->data, 0);
	return 1;
}

int dm_disconnect_block_cache(int dev)
{
	return cache_destroy_all_id(blk_cache, dev);
}

int dm_write_block_cache(int dev, u64 blk)
{
	struct ce_t *c = cache_find_element(blk_cache, dev, blk);
	dm_block_cache_sync(c);
	return 1;
}

void dm_block_cache_init()
{
#if CONFIG_MODULES
#if CONFIG_BLOCK_CACHE
	loader_add_kernel_symbol(dm_write_block_cache);
	loader_add_kernel_symbol(dm_disconnect_block_cache);
#endif
#endif
	
	blk_cache = cache_create(dm_block_cache_sync, "block");
}

int dm_cache_block(int dev, u64 blk, int sz, char *buf)
{
	return do_cache_object(blk_cache, dev < 0 ? -dev : dev, blk, sz, buf, 
		dev < 0 ? 0 : 1);
}

int dm_get_block_cache(int dev, u64 blk, char *buf)
{
	struct ce_t *c = cache_find_element(blk_cache, dev, blk);
	if(!c)
		return 0;
	memcpy(buf, c->data, c->length);
	return 1;
}

int dm_proc_read_bcache(char *buf, int off, int len)
{
	return 0;
}
#endif
