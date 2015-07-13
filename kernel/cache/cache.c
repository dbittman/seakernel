/* kernel/cache.c: Oct 2010. Copyright (c) 2010 Daniel Bittman 
 * System to cache elements, stored in a hash table and identified by
 * two different integers: id and key. They're really interchangable.
 * 
 */
#include <sea/kernel.h>
#include <sea/lib/cache.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>
#include <sea/loader/symbol.h>
#include <sea/lib/hash.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>

static struct llist *cache_list;

static void accessed_cache(cache_t *c)
{
	c->slow=100;
	c->acc=1000;
}

static int should_element_be_added(cache_t *c)
{
	if(c->count > 100000)
		return 0;
	if((pm_used_pages * 100) / pm_num_pages >= 80)
		return 0;
	return 1;
}

int init_cache(void)
{
#if CONFIG_MODULES
	loader_add_kernel_symbol(cache_create);
	loader_add_kernel_symbol(cache_find_element);
	loader_add_kernel_symbol(do_cache_object);
	loader_add_kernel_symbol(cache_remove_element);
	loader_add_kernel_symbol(cache_sync_element);
	loader_add_kernel_symbol(cache_destroy);
	loader_add_kernel_symbol(cache_sync);
	loader_add_kernel_symbol(cache_destroy_all_id);
	loader_add_kernel_symbol(cache_sync_all);
#endif
	cache_list = ll_create(0);
	return 0;
}

/* Add and remove items from the list of dirty items */
static void add_dlist(cache_t *c, struct ce_t *e)
{
	e->dirty_node = ll_insert(&c->dirty_ll, e);
}

static void remove_dlist(cache_t *c, struct ce_t *e)
{
	ll_remove(&c->dirty_ll, e->dirty_node);
}

static int set_dirty(cache_t *c, struct ce_t *e, int dirty)
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

cache_t *cache_create(cache_t *c, int (*sync)(struct ce_t *), char *name, unsigned flags)
{
	if(!c) {
		c = (void *)kmalloc(sizeof(cache_t));
		c->flags = CF_ALLOC;
	}
	else
		c->flags = 0;
	c->sync = sync;
	c->syncing=0;
	c->dirty=0;
	c->rwl = rwlock_create(0);
	c->count=0;
	c->slow=1000;
	c->hash = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(c->hash, HASH_RESIZE_MODE_IGNORE,100000);
	hash_table_specify_function(c->hash, HASH_FUNCTION_BYTE_SUM);
	strncpy(c->name, name, 32);
	ll_create(&c->dirty_ll);
	ll_create(&c->primary_ll);
	ll_insert(cache_list, c);
	
	printk(0, "[cache]: Allocated new cache '%s'\n", name);
	return c;
}

static int cache_add_element(cache_t *c, struct ce_t *obj, int locked)
{
	accessed_cache(c);
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	
	uint64_t key[2];
	key[0] = obj->id;
	key[1] = obj->key;
	hash_table_set_entry(c->hash, key, sizeof(uint64_t), 2, obj);
	obj->list_node = ll_insert(&c->primary_ll, obj);
	c->count++;
	obj->acount=1;
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
	return 0;
}

struct ce_t *cache_find_element(cache_t *c, u64 id, u64 key)
{
	accessed_cache(c);
	rwlock_acquire(c->rwl, RWL_READER);
	struct ce_t *ret;
	if(!c->hash) return 0;
	uint64_t key_arr[2];
	key_arr[0] = id;
	key_arr[1] = key;
	if(hash_table_get_entry(c->hash, key_arr, sizeof(uint64_t), 2, (void **)&ret) != 0)
		ret = 0;
	rwlock_release(c->rwl, RWL_READER);
	return ret;
}

static int do_cache_sync_element(cache_t *c, struct ce_t *e, int locked)
{
	int ret=0;
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	if(c->sync)
		ret = c->sync(e);
	set_dirty(c, e, 0);
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
	return ret;
}

int do_cache_object(cache_t *c, u64 id, u64 key, int sz, char *buf, int dirty)
{
	accessed_cache(c);
	rwlock_acquire(c->rwl, RWL_WRITER);
	struct ce_t *obj;
	uint64_t key_arr[2];
	key_arr[0] = id;
	key_arr[1] = key;
	if(hash_table_get_entry(c->hash, key_arr, sizeof(uint64_t), 2, (void **)&obj) != 0)
		obj = 0;
	if(obj)
	{
		memcpy(obj->data, buf, obj->length);
		set_dirty(c, obj, dirty);
		rwlock_release(c->rwl, RWL_WRITER);
		return 0;
	}
	if(!should_element_be_added(c))
	{
		u64 a, b;
		struct ce_t *q;
		
		if(hash_table_enumerate_entries(c->hash, 0, 0, 0, 0, (void **)&q) == 0) {
			if(q->dirty)
				do_cache_sync_element(c, q, 1);
			cache_remove_element(c, q, 1);
		}
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
void cache_remove_element(cache_t *c, struct ce_t *o, int locked)
{
	if(!o) return;
	if(o->dirty)
		panic(PANIC_NOSYNC, "tried to remove non-sync'd element");
	
	if(!locked) rwlock_acquire(c->rwl, RWL_WRITER);
	if(o->dirty)
		set_dirty(c, o, 0);
	assert(c->count);
	sub_atomic(&c->count, 1);
	ll_remove(&c->primary_ll, o->list_node);
	
	if(c->hash) {
		uint64_t key[2];
		key[0] = o->id;
		key[1] = o->key;
		hash_table_delete_entry(c->hash, key, sizeof(uint64_t), 2);
	}
	if(o->data)
		kfree(o->data);
	rwlock_destroy(o->rwl);
	kfree(o);
	if(!locked) rwlock_release(c->rwl, RWL_WRITER);
}

int cache_sync_element(cache_t *c, struct ce_t *e)
{
	return do_cache_sync_element(c, e, 0);
}

void cache_sync(cache_t *c)
{
	if(!c->dirty || !c->sync) return;
	accessed_cache(c);
	printk(0, "[cache]: Cache '%s' is syncing\n", c->name);
	unsigned int num = c->dirty;
	unsigned int i=1;
	struct ce_t *obj;
	c->syncing=1;
	while(c->dirty > 0)
	{
		rwlock_acquire(c->rwl, RWL_WRITER);
		if(c->dirty == 0) {
			c->syncing = 0;
			rwlock_release(c->rwl, RWL_WRITER);
			break;
		}
		assert(c->dirty_ll.head);
		obj = c->dirty_ll.head->entry;
		if(num < (c->dirty+i))
			num=(c->dirty+i);
		
		printk((kernel_state_flags & KSF_SHUTDOWN) ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)...   "
				,c->name, i, num, (i*100)/num, ((i*1000)/num) % 10);
		
		do_cache_sync_element(c, obj, 1);
		rwlock_release(c->rwl, RWL_WRITER);
		
		if(tm_thread_got_signal(current_thread))
			return;
		i++;
	}
	
	c->syncing=0;
	printk((kernel_state_flags & KSF_SHUTDOWN) ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)\n"
			, c->name, num, num, 100, 0);
	printk(0, "[cache]: Cache '%s' has sunk\n", c->name);
}

int cache_destroy_all_id(cache_t *c, u64 id)
{
	rwlock_acquire(c->rwl, RWL_WRITER);
	struct llistnode *curnode, *next;
	struct ce_t *obj;
	ll_for_each_entry_safe(&c->primary_ll, curnode, next, struct ce_t *, obj)
	{
		if(obj->id == id)
		{
			if(obj->dirty)
				do_cache_sync_element(c, obj, 1);
			cache_remove_element(c, obj, 1);
		}
	}
	rwlock_release(c->rwl, RWL_WRITER);
	return 0;
}

void cache_destroy(cache_t *c)
{
	printk(1, "[cache]: Destroying cache '%s'...\n", c->name);
	rwlock_acquire(c->rwl, RWL_WRITER);
	struct hash_table *h = c->hash;
	c->hash = 0;
	cache_sync(c);
	hash_table_destroy(h);
	
	struct llistnode *curnode, *next;
	struct ce_t *obj;
	ll_for_each_entry_safe(&c->primary_ll, curnode, next, struct ce_t *, obj)
	{
		cache_remove_element(c, obj, 1);
	}
	ll_destroy(&c->dirty_ll);
	ll_destroy(&c->primary_ll);
	ll_remove_entry(cache_list, c);
	rwlock_release(c->rwl, RWL_WRITER);
	rwlock_destroy(c->rwl);
	if(c->flags & CF_ALLOC)
		kfree(c);
	printk(1, "[cache]: Cache '%s' destroyed\n", c->name);
}

int cache_sync_all(void)
{
	struct llistnode *cur;
	cache_t *ent;
	rwlock_acquire(&cache_list->rwl, RWL_READER);
	ll_for_each_entry(cache_list, cur, cache_t *, ent)
	{
		cache_sync(ent);
	}
	rwlock_release(&cache_list->rwl, RWL_READER);
	return 0;
}
