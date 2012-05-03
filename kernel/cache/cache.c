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
cache_t caches[NUM_CACHES];
void accessed_cache(int c)
{
	caches[c].slow=100;
	caches[c].acc=1000;
}

int init_cache()
{
	memset(caches, 0, NUM_CACHES * sizeof(cache_t));
	return 0;
}

/* Add and remove items from the list of dirty items */
void add_dlist(int c, struct ce_t *e)
{
	struct ce_t *tmp = caches[c].dlist;
	caches[c].dlist = e;
	e->next_dirty = tmp;
	e->prev_dirty=0;
	if(tmp)
		tmp->prev_dirty=e;
}

void remove_dlist(int c, struct ce_t *e)
{
	if(e->prev_dirty)
		e->prev_dirty->next_dirty = e->next_dirty;
	if(e->next_dirty)
		e->next_dirty->prev_dirty = e->prev_dirty;
	if(caches[c].dlist == e)
	{
		assert(!e->prev_dirty);
		caches[c].dlist = (e->next_dirty);
	}
	e->prev_dirty=e->next_dirty=0;
}

int set_dirty(int c, struct ce_t *e, int dirty)
{
	assert(e && caches[c].flag);
	mutex_on(&caches[c].dlock);
	int old = e->dirty;
	e->dirty=dirty;
	if(dirty)
	{
		if(old != dirty) {
			add_dlist(c, e);
			caches[c].dirty++;
		}
	} else
	{
		if(old != dirty) {
			assert(caches[c].dirty);
			caches[c].dirty--;
			remove_dlist(c, e);
		}
	}
	mutex_off(&caches[c].dlock);
	return old;
}

int get_empty_cache(int (*sync)(struct ce_t *))
{
	int i;
	for(i=0;i<NUM_CACHES && caches[i].flag;i++);
	if(i == NUM_CACHES)
		panic(0, "Ran out of cash! Can I have some money please?");
	caches[i].flag=1;
	caches[i].sync = sync;
	caches[i].syncing=0;
	caches[i].dirty=0;
	create_mutex(&caches[i].lock);
	create_mutex(&caches[i].dlock);
	caches[i].count=0;
	caches[i].slow=1000;
	
	caches[i].hash = chash_create(100000);
	
	printk(0, "[cache]: Allocated new cache %d\n", i);
	return i;
}

int cache_add_element(int c, struct ce_t *obj)
{
	assert(caches[c].flag);
	accessed_cache(c);
	mutex_on(&caches[c].lock);
	
	chash_add(caches[c].hash, obj->id, obj->key, obj);
	
	struct ce_t *old = caches[c].list;
	obj->next = old;
	if(old) old->prev = obj;
	else    caches[c].list = caches[c].tail = obj;
	obj->prev = 0;
	caches[c].count++;
	obj->acount=1;
	mutex_off(&caches[c].lock);
	return 0;
}

struct ce_t *find_cache_element(int c, unsigned id, unsigned key)
{
	accessed_cache(c);
	mutex_on(&caches[c].lock);
	
	struct ce_t *ret = chash_search(caches[c].hash, id, key);
	
	if(ret)
		ret->acount++;
	mutex_off(&caches[c].lock);
	return ret;
}

int do_cache_object(int c, unsigned id, unsigned key, int sz, char *buf, int dirty)
{
	mutex_on(&caches[c].lock);
	struct ce_t *obj = find_cache_element(c, id, key);
	if(obj)
	{
		memcpy(obj->data, buf, obj->length);
		set_dirty(c, obj, dirty);
		mutex_off(&caches[c].lock);
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
	mutex_off(&caches[c].lock);
	return 0;
}

/* WARNING: This does not sync!!! */
void remove_element(int c, struct ce_t *o)
{
	if(!o) return;
	if(o->dirty)
		printk(1, "[cache]: Warning - Removing non-sync'd element %d in cache %d\n", o->key, c);
	
	mutex_on(&caches[c].lock);
	if(o->dirty)
		set_dirty(c, o, 0);
	assert(caches[c].count);
	caches[c].count--;
	
	if(o->prev)
		o->prev->next = o->next;
	else
		caches[c].list = o->next;
	if(o->next)
		o->next->prev = o->prev;
	else
		caches[c].tail = o->prev;
	chash_delete(caches[c].hash, o->id, o->key);
	if(o->data)
		kfree(o->data);
	kfree(o);
	mutex_off(&caches[c].lock);
}

void remove_element_byid(int c, unsigned id, unsigned key)
{
	mutex_on(&caches[c].lock);
	struct ce_t *o = find_cache_element(c, id, key);
	remove_element(c, o);
	mutex_off(&caches[c].lock);
}

void try_remove_element(int c, struct ce_t* o)
{
	if(o->dirty) return;
	mutex_on(&caches[c].lock);
	remove_element(c, o);
	mutex_off(&caches[c].lock);
}

int sync_element(int c, struct ce_t *e)
{
	if(!caches[c].flag)
		return 0;
	int ret=0;
	if(caches[c].sync)
		ret = caches[c].sync(e);
	set_dirty(c, e, 0);
	return ret;
}

int destroy_cache(int id, int slow)
{
	/* Sync with forced removal */
	printk(1, "[cache]: Destroying cache %d...\n", id);
	sync_cache(id, 0, 0, 1);
	/* Destroy the tree */
	mutex_on(&caches[id].lock);
	
	chash_destroy(caches[id].hash);
	
	mutex_off(&caches[id].lock);
	destroy_mutex(&caches[id].lock);
	destroy_mutex(&caches[id].dlock);
	caches[id].flag=0;
	printk(1, "[cache]: Cache %d destroyed\n", id);
	return 1;
}

void do_sync_cache(int id, int red, volatile int slow, int rm)
{
	if(!caches[id].flag || !caches[id].dirty || !caches[id].sync) return;
	accessed_cache(id);
	printk(0, "[cache]: Cache %d is syncing", id);
	if(slow)
		printk(0, " slowly");
	printk(0, " with force = %d\n", rm);
	top:
	__super_sti();
	volatile unsigned int num = caches[id].dirty;
	volatile unsigned int i=1;
	struct ce_t *obj;
	if(!slow)
		caches[id].syncing=1;
	while(1)
	{
		if(!caches[id].dlist) break;
		
		mutex_on(&caches[id].lock);
		mutex_on(&caches[id].dlock);
		obj = caches[id].dlist;
		if(!obj) {
			mutex_off(&caches[id].dlock);
			mutex_off(&caches[id].lock);
			caches[id].syncing=0;
			break;
		}
		if(num < (caches[id].dirty+i))
			num=(caches[id].dirty+i);
		if(!slow)
			printk(red, "\r[cache]: Syncing %d: %d/%d (%d.%d%%)...   ", id, i, num, (i*100)/num, ((i*1000)/num) % 10);
		sync_element(id, obj);
		mutex_off(&caches[id].dlock);
		mutex_off(&caches[id].lock);
		
		if(slow) {
			wait_flag(&caches[id].syncing, 0);
			if(got_signal(current_task)) {
					return;
			}
			unsigned d = caches[id].slow;
			if(d < 10) d = 10;
			caches[id].slow /= 2;
			delay_sleep(d);
		}
		if(got_signal(current_task)) {
			return;
		}
		i++;
	}
	
	if(!slow)
	{
		caches[id].syncing=0;
		printk(red, "\r[cache]: Syncing %d: %d/%d (%d.%d%%)\n", id, num, num, 100, 0);
	}
	printk(0, "[cache]: Cache %d has sunk\n", id);
}

void sync_cache(int id, int red, int slow, int rm)
{
	do_sync_cache(id, red, slow, rm);
}

int kernel_cache_sync(int all, int disp)
{
	int i;
	for(i=0;i<NUM_CACHES;i++)
	{
		if(caches[i].flag)
			sync_cache(i, disp, 0, 1);
	}
	return 0;
}

int kernel_cache_sync_slow(int all)
{
	int i;
	for(i=0;i<NUM_CACHES;i++)
	{
		if(caches[i].flag)
			sync_cache(i, 0, 1, 1);
	}
	return 0;
}

int reclaim_cache_memory(int i)
{
	struct ce_t *del = caches[i].tail;
	while(del && del->dirty) 
		del=del->prev;
	if(!del || del->dirty) return 0;
	remove_element(i, del);
	return 1;
}

extern volatile unsigned pm_num_pages, pm_used_pages;
int __KT_cache_reclaim_memory()
{
	int i;
	int usage = (pm_used_pages * 100)/(pm_num_pages);
	int total=0;
	for(i=0;i<NUM_CACHES;i++)
	{
		if(!caches[i].flag)
			continue;
		if((caches[i].count < CACHE_CAP) && usage < 30)
			continue;
		if(caches[i].flag) {
			mutex_on(&caches[i].lock);
			mutex_on(&caches[i].dlock);
			
			total+=reclaim_cache_memory(i);
			
			mutex_off(&caches[i].dlock);
			mutex_off(&caches[i].lock);
		}
	}
	return total;
}
