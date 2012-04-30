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

cache_t caches[NUM_CACHES];
struct cache_map cm[NUM_CACHES];

void accessed_cache(int c)
{
	caches[c].slow=100;
	caches[c].acc=1000;
}

bptree *cache_get_btree(unsigned id, unsigned key)
{
	return caches[id].bt[key%NUM_TREES];
}

int init_cache()
{
	memset(caches, 0, NUM_CACHES * sizeof(cache_t));
	memset(cm, 0, NUM_CACHES*sizeof(struct cache_map));
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

int get_empty_cache(int (*sync)(struct ce_t *), int num_obj, int (*s_m)(int, struct ce_t *, char *, int))
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
	caches[i].nrobj = num_obj;
	caches[i].slow=1000;
	caches[i].sync_multiple=s_m;
	int k=0;
	while(k<NUM_TREES)
	{
		caches[i].bt[k] = bptree_create();
		k++;
	}
	printk(0, "[cache]: Allocated new cache %d\n", i);
	return i;
}

int cache_add_element(int c, struct ce_t *obj)
{
	assert(caches[c].flag);
	assert(caches[c].count < caches[c].nrobj);
	accessed_cache(c);
	mutex_on(&caches[c].lock);
	bptree_insert(cache_get_btree(c, obj->key), obj->key, obj);
	caches[c].count++;
	obj->acount=1;
	mutex_off(&caches[c].lock);
	return 0;
}

struct ce_t *find_cache_element(int c, unsigned id, char *name)
{
	accessed_cache(c);
	mutex_on(&caches[c].lock);
	if(caches[c].last && caches[c].last->key == id)
	{
		struct ce_t *ret_l = caches[c].last;
		mutex_off(&caches[c].lock);
		return ret_l;
	}
	unsigned min = bptree_get_min_key(cache_get_btree(c, id));
	unsigned max = bptree_get_max_key(cache_get_btree(c, id));
	if(id > max || id < min) {
		mutex_off(&caches[c].lock);
		return 0;
	}
	struct ce_t *ret = bptree_search(cache_get_btree(c, id), id);
	if(!ret) {
		mutex_off(&caches[c].lock);
		return 0;
	}
	ret->acount++;
	caches[c].last = ret;
	mutex_off(&caches[c].lock);
	return ret;
}

void cache_update_obj(int c, struct ce_t *obj, unsigned sz, char *buf)
{
	if(obj->length < sz || !obj->data)
	{
		if(obj->data)
			kfree(obj->data);
		obj->data = (char *)kmalloc(sz);
	}
	memcpy(obj->data, buf, sz);
}

int do_cache_object(int c, int id, char *name, int sz, char *buf, int dirty)
{
	mutex_on(&caches[c].lock);
	struct ce_t *o;
	o = find_cache_element(c, id, name);
	if(o)
	{
		cache_update_obj(c, o, sz, buf);
		set_dirty(c, o, dirty);
		mutex_off(&caches[c].lock);
		return 0;
	}
	struct ce_t *obj = (struct ce_t *)kmalloc(sizeof(struct ce_t));
	obj->data = (char *)kmalloc(sz);
	obj->length = sz;
	memcpy(obj->data, buf, sz);
	if(name) strncpy(obj->name, name, 31);
	set_dirty(c, obj, dirty);
	obj->key = id;
	obj->flag=1;
	cache_add_element(c, obj);
	mutex_off(&caches[c].lock);
	return 0;
}

int cache_object(int c, int id, char *name, int sz, char *buf)
{
	return do_cache_object(c, id, name, sz, buf, 1);
}

int cache_object_clean(int c, int id, char *name, int sz, char *buf)
{
	return do_cache_object(c, id, name, sz, buf, 0);
}

/* WARNING: This does not sync!!! */
void remove_element(int c, struct ce_t *o)
{
	if(!o) return;
	if(o->dirty)
		printk(1, "[cache]: Warning - Removing non-sync'd element %d in cache %d\n", o->key, c);
	mutex_on(&caches[c].lock);
	if(caches[c].last && caches[c].last->key == o->key)
		caches[c].last=0;
	if(o->dirty)
		set_dirty(c, o, 0);
	assert(caches[c].count);
	caches[c].count--;
	bptree_delete(cache_get_btree(c, o->key), o->key);
	if(o->data)
		kfree(o->data);
	kfree(o);
	mutex_off(&caches[c].lock);
}

void remove_element_byid(int c, int id)
{
	struct ce_t *o;
	mutex_on(&caches[c].lock);
	o = find_cache_element(c, id, 0);
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
	if(!(caches[c].flag && e->flag))
		return 0;
	int ret=0;
	if(caches[c].sync)
		ret = caches[c].sync(e);
	set_dirty(c, e, 0);
	return ret;
}
extern char shutting_down;
int destroy_cache(int id, int slow)
{
	/* Sync with forced removal */
	printk(1, "[cache]: Destroying cache %d...\n", id);
	sync_cache(id, 0, 0, 1);
	/* Destroy the tree */
	mutex_on(&caches[id].lock);
	int k=0;
	while(k<NUM_TREES) {
		bptree_destroy(caches[id].bt[k]);
		kfree(caches[id].bt[k]);
		k++;
	}
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

int reclaim_cache_memory(int id, unsigned time)
{
	unsigned ctime = get_epoch_time();
	int num=0;
	int k=caches[id].l_rc++;
	if(k >= NUM_TREES)
		k=0, caches[id].l_rc=1;
	
	unsigned min = bptree_get_min_key(caches[id].bt[k]);
	unsigned max = bptree_get_max_key(caches[id].bt[k]);
	if(min == max || (!min && k) || !max)
		return num;
	unsigned i = max;
	struct ce_t *obj = bptree_search(caches[id].bt[k], i);
	if(obj && !obj->dirty && obj->atime < (ctime-time)) {
		num++;
		caches[id].last=0;
		remove_element(id, obj);
	}
	return num;
}

extern volatile unsigned pm_num_pages, pm_used_pages;
int __KT_cache_reclaim_memory()
{
	int i;
	int usage = (pm_used_pages * 100)/(pm_num_pages);
	int del=1+80/(usage);
	int total=0;
	for(i=0;i<NUM_CACHES;i++)
	{
		if(!caches[i].flag)
			continue;
		if((caches[i].count < CACHE_CAP) && usage < 30)
			continue;
		int d = del;
		if(caches[i].count > CACHE_CAP && d > 5)
			d=5;
		if(caches[i].flag) {
			mutex_on(&caches[i].lock);
			mutex_on(&caches[i].dlock);
			total+=(reclaim_cache_memory(i, d));
			mutex_off(&caches[i].dlock);
			mutex_off(&caches[i].lock);
			delay((caches[i].acc/=2) + 10);
		}
	}
	return total;
}
