/* Provides access layer to the kernel cache for block devices (write-through block cache. Speeds up writing */
#include <kernel.h>
#include <dev.h>
#include <block.h>
#include <cache.h>

struct ctbl {
	int cn;
	int dev;
	int sz;
	struct ctbl *next;
} *c_table=0;
mutex_t cndev_m;

int get_cndev(int dev, int *sz)
{
	mutex_on(&cndev_m);
	struct ctbl *c = c_table;
	while(c && c->dev != dev) c=c->next;
	if(c && sz)
		*sz = c->sz;
	mutex_off(&cndev_m);
	return c ? c->cn : -1;
}

int add_cndev(int c, int dev, int sz)
{
	mutex_on(&cndev_m);
	struct ctbl *new = (struct ctbl *)kmalloc(sizeof(struct ctbl));
	new->cn = c;
	new->sz=sz;
	new->dev=dev;
	struct ctbl *old = c_table;
	c_table = new;
	new->next=old;
	mutex_off(&cndev_m);
	return 0;
}

int rem_cndev(int dev)
{
	mutex_on(&cndev_m);
	struct ctbl *c = c_table;
	struct ctbl *p = 0;
	while(c && c->dev != dev) {
		p=c;
		c=c->next;
	}
	if(c) {
		if(p)
		{
			p->next = c->next;
		} else
		{
			assert(c_table == c);
			c_table = c->next;
		}
		kfree(c);
	}
	mutex_off(&cndev_m);
	return (int)c;
}

void block_cache_init()
{
	create_mutex(&cndev_m);
}

int block_cache_sync(struct ce_t *c)
{
	int dev = strtoint(c->name);
	int blk = c->key;
	if(c->dirty)
		do_block_rw(WRITE, dev, blk, c->data, 0);
	return 1;
}

int sync_block_device(int dev)
{
	int i = get_cndev(dev, 0);
	if(i == -1)
		return 0;
	sync_cache(i, 0, 0, 2);
	return 1;
}

int disconnect_block_cache(int dev)
{
	int x = get_cndev(dev, 0);
	if(x == -1)
		return 0;
	rem_cndev(dev);
	sync_cache(x, 0, 0, 2);
	destroy_cache(x, 0);
	return 1;
}

int disconnect_block_cache_1(int dev)
{
	int x = get_cndev(dev, 0);
	if(x == -1)
		return -1;
	rem_cndev(dev);
	return x;
}

int disconnect_block_cache_2(int x)
{
	sync_cache(x, 0, 0, 2);
	destroy_cache(x, 0);
	return 1;
}

int disconnect_block_cache_slow(int dev)
{
	int x = get_cndev(dev, 0);
	if(x == -1)
		return 0;
	rem_cndev(dev);
	sync_cache(x, 0, 0, 2);
	destroy_cache(x, 1);
	return 1;
}

int cache_block(int dev, unsigned blk, int sz, char *buf)
{
	int nd=0;
	if(dev < 0)
	{
		nd=1;
		dev=-dev;
	}
	int cn=-1;
	if((cn=get_cndev(dev, 0)) == (-1))
		add_cndev(cn=get_empty_cache(block_cache_sync, 0x100000, 0), dev, sz);
	assert(cn != -1);
	char d[8];
	sprintf(d, "%d", dev);
	if(nd) {
		return cache_object_clean(cn, blk, d, sz, buf);
	}
	return cache_object(cn, blk, d, sz, buf);
}

int get_block_cache(int dev, int blk, char *buf)
{
	int cn=-1;
	int sz;
	if((cn=get_cndev(dev, &sz)) == -1)
		return 0;
	int base = blk;
	struct ce_t *c = find_cache_element(cn, base, 0);
	if(!c)
		return 0;
	lock_scheduler();
	memcpy(buf, (c->data), sz);
	c->atime = get_epoch_time();
	unlock_scheduler();
	return 1;
}

int write_block_cache(int dev, int blk)
{
	int cn=-1;
	if((cn=get_cndev(dev, 0)) == -1)
		return 0;
	struct ce_t *c = find_cache_element(cn, blk, 0);
	block_cache_sync(c);
	return 1;
}

int proc_read_bcache(char *buf, int off, int len)
{
	int i, total_len=0;
	total_len += proc_append_buffer(buf, "MAJ:MIN | DIRTY | SIZE (KB) | SYNCING\n", total_len, -1, off, len);
	struct ctbl *c = c_table;
	while(c)
	{
		if(c->cn != -1)
		{
			char t[128];
			sprintf(t, "%3d:%-3d | %5d | %9d | %s\n", MAJOR(c->dev), MINOR(c->dev), caches[c->cn].dirty, (caches[c->cn].count * c->sz)/1024, caches[c->cn].syncing ? "yes" : "no");
			total_len += proc_append_buffer(buf, t, total_len, -1, off, len);
		}
		c=c->next;
	}
	return total_len;
}
