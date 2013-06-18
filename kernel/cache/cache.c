/* kernel/cache.c: Oct 2010. Copyright (c) 2010 Daniel Bittman 
 * System to cache elements, stored in a hash table and identified by
 * two different integers: id and key. They're really interchangable.
 * 
 */
#include <kernel.h>
#include <cache.h>
#include <task.h>
#include <ll.h>
#include <atomic.h>
#include <mod.h>
struct llist *cache_list;
int disconnect_block_cache(int dev);
int write_block_cache(int dev, u64 blk);
void accessed_cache(cache_t *c)
{
	c->slow=100;
	c->acc=1000;
}

int init_cache()
{
#if CONFIG_MODULES
	add_kernel_symbol(get_empty_cache);
	add_kernel_symbol(find_cache_element);
	add_kernel_symbol(do_cache_object);
	add_kernel_symbol(remove_element);
	add_kernel_symbol(sync_element);
	add_kernel_symbol(destroy_cache);
	add_kernel_symbol(sync_cache);
	add_kernel_symbol(write_block_cache);
	add_kernel_symbol(disconnect_block_cache);
	add_kernel_symbol(destroy_all_id);
	add_kernel_symbol(kernel_cache_sync);
#endif
	cache_list = ll_create(0);
	return 0;
}

/* Add and remove items from the list of dirty items */
void add_dlist(cache_t *c, struct ce_t *e)
{
	struct ce_t *tmp = c->dlist;
	c->dlist = e;
	e->next_dirty = tmp;
	e->prev_dirty=0;
	if(tmp)
		tmp->prev_dirty=e;
}

void remove_dlist(cache_t *c, struct ce_t *e)
{
	if(e->prev_dirty)
		e->prev_dirty->next_dirty = e->next_dirty;
	if(e->next_dirty)
		e->next_dirty->prev_dirty = e->prev_dirty;
	if(c->dlist == e)
	{
		assert(!e->prev_dirty);
		c->dlist = e->next_dirty;
	}
	e->prev_dirty=e->next_dirty=0;
}

int set_dirty(cache_t *c, struct ce_t *e, int dirty)
{
	int old = e->dirty;
	e->dirty=dirty;
	if(dirty)
	{
		if(old != dirty) {
			add_dlist(c, e);
			add_atomic(&c->dirty, 1);
		}
	} else
	{
		if(old != dirty) {
			assert(c->dirty);
			sub_atomic(&c->dirty, 1);
			remove_dlist(c, e);
		}
	}
	return old;
}

cache_t *get_empty_cache(int (*sync)(struct ce_t *), char *name)
{
	cache_t *c = (void *)kmalloc(sizeof(cache_t));
	c->sync = sync;
	c->syncing=0;
	c->dirty=0;
	c->rwl = rwlock_create(0);
	c->count=0;
	c->slow=1000;
	c->hash = chash_create(100000);
	strncpy(c->name, name, 32);
	
	ll_insert(cache_list, c);
	
	printk(0, "[cache]: Allocated new cache '%s'\n", name);
	return c;
}

int cache_add_element(cache_t *c, struct ce_t *obj, int locked)
{
	accessed_cache(c);
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	
	chash_add(c->hash, obj->id, obj->key, obj);
	
	struct ce_t *old = c->list;
	obj->next = old;
	if(old) old->prev = obj;
	else    c->list = c->tail = obj;
	obj->prev = 0;
	c->count++;
	obj->acount=1;
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
	return 0;
}

struct ce_t *find_cache_element(cache_t *c, u64 id, u64 key)
{
	accessed_cache(c);
	rwlock_acquire(c->rwl, RWL_READER);
	struct ce_t *ret = c->hash ? chash_search(c->hash, id, key) : 0;
	rwlock_release(c->rwl, RWL_READER);
	return ret;
}

int do_cache_object(cache_t *c, u64 id, u64 key, int sz, char *buf, int dirty)
{
	accessed_cache(c);
	rwlock_acquire(c->rwl, RWL_WRITER);
	struct ce_t *obj = chash_search(c->hash, id, key);
	if(obj)
	{
		memcpy(obj->data, buf, obj->length);
		set_dirty(c, obj, dirty);
		rwlock_release(c->rwl, RWL_WRITER);
		return 0;
	}
	obj = (struct ce_t *)kmalloc(sizeof(struct ce_t));
	obj->data = (char *)kmalloc(sz);
	obj->length = sz;
	obj->rwl = rwlock_create(0);
	memcpy(obj->data, buf, sz);
	obj->key = key;
	obj->id = id;
	set_dirty(c, obj, dirty);
	cache_add_element(c, obj, 1);
	rwlock_release(c->rwl, RWL_WRITER);
	return 0;
}

/* WARNING: This does not sync!!! */
void remove_element(cache_t *c, struct ce_t *o, int locked)
{
	if(!o) return;
	if(o->dirty)
		panic(PANIC_NOSYNC, "tried to remove non-sync'd element");
	
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	if(o->dirty)
		set_dirty(c, o, 0);
	assert(c->count);
	sub_atomic(&c->count, 1);
	
	if(o->prev)
		o->prev->next = o->next;
	else
		c->list = o->next;
	if(o->next)
		o->next->prev = o->prev;
	else
		c->tail = o->prev;
	if(c->hash) chash_delete(c->hash, o->id, o->key);
	if(o->data)
		kfree(o->data);
	rwlock_destroy(o->rwl);
	kfree(o);
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
}

int do_sync_element(cache_t *c, struct ce_t *e, int locked)
{
	int ret=0;
	rwlock_acquire(e->rwl, RWL_READER);
	if(c->sync)
		ret = c->sync(e);
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	set_dirty(c, e, 0);
	rwlock_release(e->rwl, RWL_READER);
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
	return ret;
}

int sync_element(cache_t *c, struct ce_t *e)
{
	return do_sync_element(c, e, 0);
}

void sync_cache(cache_t *c)
{
	if(!c->dirty || !c->sync) return;
	accessed_cache(c);
	printk(0, "[cache]: Cache '%s' is syncing\n", c->name);
	volatile unsigned int num = c->dirty;
	volatile unsigned int i=1;
	struct ce_t *obj;
	c->syncing=1;
	while(1)
	{
		if(!c->dlist) break;
		rwlock_acquire(c->rwl, RWL_WRITER);
		obj = c->dlist;
		if(!obj) {
			rwlock_release(c->rwl, RWL_WRITER);
			c->syncing=0;
			break;
		}
		if(num < (c->dirty+i))
			num=(c->dirty+i);
		
		printk((kernel_state_flags & KSF_SHUTDOWN) ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)...   "
				,c->name, i, num, (i*100)/num, ((i*1000)/num) % 10);
		
		do_sync_element(c, obj, 1);
		rwlock_release(c->rwl, RWL_WRITER);
		
		if(got_signal(current_task)) {
			return;
		}
		i++;
	}
	
	c->syncing=0;
	printk((kernel_state_flags & KSF_SHUTDOWN) ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)\n"
			, c->name, num, num, 100, 0);
	printk(0, "[cache]: Cache '%s' has sunk\n", c->name);
}

int destroy_all_id(cache_t *c, u64 id)
{
	rwlock_acquire(c->rwl, RWL_WRITER);
	struct ce_t *o = c->list;
	while(o) {
		if(o->id == id) {
			if(o->dirty)
				do_sync_element(c, o, 1);
			remove_element(c, o, 1);
		}
		o=o->next;
	}
	rwlock_release(c->rwl, RWL_WRITER);
	return 0;
}

int destroy_cache(cache_t *c)
{
	/* Sync with forced removal */
	printk(1, "[cache]: Destroying cache '%s'...\n", c->name);
	chash_t *h = c->hash;
	c->hash = 0;
	sync_cache(c);
	/* Destroy the tree */
	chash_destroy(h);
	rwlock_destroy(c->rwl);
	ll_remove_entry(cache_list, c);
	printk(1, "[cache]: Cache '%s' destroyed\n", c->name);
	return 1;
}

int kernel_cache_sync()
{
	struct llistnode *cur;
	cache_t *ent;
	rwlock_acquire(&cache_list->rwl, RWL_READER);
	ll_for_each_entry(cache_list, cur, cache_t *, ent)
	{
		sync_cache(ent);
	}
	rwlock_release(&cache_list->rwl, RWL_READER);
	return 0;
}
