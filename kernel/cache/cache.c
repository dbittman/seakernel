/* kernel/cache.c: Oct 2010. Copyright (c) 2010 Daniel Bittman 
 * This provides a general abstraction layer to a btree system, where they data values are pointers to structs.
 * These structs contain information on storing data of specified sizes. Essentially, a kernel cache system of 
 * data. Primarily used in block cache and FS caches.
 * 
 * The reclaiming system is pretty simple. If there is more than 20000 elements in a cache, the kernel task is
 * allowed to reclaim old elements (that haven't been accessed for > some number of seconds). It can also reclaim
 * elements if the memory usage of the system to deemed too high.
 */
#include <kernel.h>
#include <cache.h>
#include <task.h>
extern char shutting_down;
mutex_t cl_mutex;
cache_t *cache_list=0;
void accessed_cache(cache_t *c)
{
	c->slow=100;
	c->acc=1000;
}

int init_cache()
{
	create_mutex(&cl_mutex);
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
	mutex_on(&c->dlock);
	int old = e->dirty;
	e->dirty=dirty;
	if(dirty)
	{
		if(old != dirty) {
			add_dlist(c, e);
			c->dirty++;
		}
	} else
	{
		if(old != dirty) {
			assert(c->dirty);
			c->dirty--;
			remove_dlist(c, e);
		}
	}
	mutex_off(&c->dlock);
	return old;
}

cache_t *get_empty_cache(int (*sync)(struct ce_t *), char *name)
{
	cache_t *c = (void *)kmalloc(sizeof(cache_t));
	c->sync = sync;
	c->syncing=0;
	c->dirty=0;
	create_mutex(&c->lock);
	create_mutex(&c->dlock);
	c->count=0;
	c->slow=1000;
	c->hash = chash_create(100000);
	strncpy(c->name, name, 32);
	
	mutex_on(&cl_mutex);
	c->next = cache_list;
	if(cache_list) cache_list->prev = c;
	cache_list = c;
	mutex_off(&cl_mutex);
	
	printk(0, "[cache]: Allocated new cache '%s'\n", name);
	return c;
}

int cache_add_element(cache_t *c, struct ce_t *obj)
{
	accessed_cache(c);
	mutex_on(&c->lock);
	
	chash_add(c->hash, obj->id, obj->key, obj);
	
	struct ce_t *old = c->list;
	obj->next = old;
	if(old) old->prev = obj;
	else    c->list = c->tail = obj;
	obj->prev = 0;
	c->count++;
	obj->acount=1;
	mutex_off(&c->lock);
	return 0;
}

struct ce_t *find_cache_element(cache_t *c, unsigned id, unsigned key)
{
	accessed_cache(c);
	mutex_on(&c->lock);
	struct ce_t *ret = chash_search(c->hash, id, key);
	if(ret)
		ret->acount++;
	mutex_off(&c->lock);
	return ret;
}

int do_cache_object(cache_t *c, unsigned id, unsigned key, int sz, char *buf, int dirty)
{
	mutex_on(&c->lock);
	struct ce_t *obj = find_cache_element(c, id, key);
	if(obj)
	{
		memcpy(obj->data, buf, obj->length);
		set_dirty(c, obj, dirty);
		mutex_off(&c->lock);
		return 0;
	}
	obj = (struct ce_t *)kmalloc(sizeof(struct ce_t));
	obj->data = (char *)kmalloc(sz);
	obj->length = sz;
	memcpy(obj->data, buf, sz);
	set_dirty(c, obj, dirty);
	obj->key = key;
	obj->id = id;
	cache_add_element(c, obj);
	mutex_off(&c->lock);
	return 0;
}

/* WARNING: This does not sync!!! */
void remove_element(cache_t *c, struct ce_t *o)
{
	if(!o) return;
	if(o->dirty)
		panic(PANIC_NOSYNC, "tried to remove non-sync'd element");
	
	mutex_on(&c->lock);
	if(o->dirty)
		set_dirty(c, o, 0);
	assert(c->count);
	c->count--;
	
	if(o->prev)
		o->prev->next = o->next;
	else
		c->list = o->next;
	if(o->next)
		o->next->prev = o->prev;
	else
		c->tail = o->prev;
	chash_delete(c->hash, o->id, o->key);
	if(o->data)
		kfree(o->data);
	kfree(o);
	mutex_off(&c->lock);
}

void remove_element_byid(cache_t *c, unsigned id, unsigned key)
{
	mutex_on(&c->lock);
	struct ce_t *o = find_cache_element(c, id, key);
	remove_element(c, o);
	mutex_off(&c->lock);
}

void try_remove_element(cache_t *c, struct ce_t* o)
{
	if(o->dirty) return;
	mutex_on(&c->lock);
	remove_element(c, o);
	mutex_off(&c->lock);
}

int sync_element(cache_t *c, struct ce_t *e)
{
	int ret=0;
	if(c->sync)
		ret = c->sync(e);
	set_dirty(c, e, 0);
	return ret;
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
		
		mutex_on(&c->lock);
		mutex_on(&c->dlock);
		obj = c->dlist;
		if(!obj) {
			mutex_off(&c->dlock);
			mutex_off(&c->lock);
			c->syncing=0;
			break;
		}
		if(num < (c->dirty+i))
			num=(c->dirty+i);
		
		printk(shutting_down ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)...   ", c->name, i, num, (i*100)/num, ((i*1000)/num) % 10);
		
		sync_element(c, obj);
		mutex_off(&c->dlock);
		mutex_off(&c->lock);
		
		if(got_signal(current_task)) {
			return;
		}
		i++;
	}
	
	c->syncing=0;
	printk(shutting_down ? 4 : 0, "\r[cache]: Syncing '%s': %d/%d (%d.%d%%)\n", c->name, num, num, 100, 0);
	printk(0, "[cache]: Cache '%s' has sunk\n", c->name);
}

int destroy_cache(cache_t *c)
{
	/* Sync with forced removal */
	printk(1, "[cache]: Destroying cache '%s'...\n", c->name);
	sync_cache(c);
	/* Destroy the tree */
	mutex_on(&c->lock);
	
	chash_destroy(c->hash);
	
	mutex_off(&c->lock);
	destroy_mutex(&c->lock);
	destroy_mutex(&c->dlock);
	mutex_on(&cl_mutex);
	if(c->prev)
		c->prev->next = c->next;
	else
		cache_list = c->next;
	if(c->next) 
		c->next->prev = c->prev;
	mutex_off(&cl_mutex);
	printk(1, "[cache]: Cache '%s' destroyed\n", c->name);
	return 1;
}

int kernel_cache_sync()
{
	cache_t *c = cache_list;
	while(c) {
		sync_cache(c);
		c=c->next;
	}
	return 0;
}

int reclaim_cache_memory(cache_t *c)
{
	struct ce_t *del = c->tail;
	while(del && del->dirty) 
		del=del->prev;
	if(!del || del->dirty) return 0;
	remove_element(c, del);
	return 1;
}

extern volatile unsigned pm_num_pages, pm_used_pages;
int __KT_cache_reclaim_memory()
{
	return 0;
	int i;
	int usage = (pm_used_pages * 100)/(pm_num_pages);
	int total=0;
	cache_t *c = cache_list;
	while(c) {
		if((c->count < CACHE_CAP) && usage < 30)
			continue;
		mutex_on(&c->lock);
		mutex_on(&c->dlock);
		
		total+=reclaim_cache_memory(c);
		
		mutex_off(&c->dlock);
		mutex_off(&c->lock);
		c=c->next;
	}
	return total;
}
