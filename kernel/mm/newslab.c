#include <sea/types.h>
#include <sea/ll.h>
#include <sea/mutex.h>
#include <sea/mm/valloc.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
struct slab {
	struct cache *cache;
	struct llistnode node;
	struct valloc allocator;
	int count, max;
	uint32_t magic;
};

struct cache {
	struct llist empty, partial, full;
	size_t object_size;
	mutex_t lock;
};

#define SLAB_SIZE 0x20000
#define SLAB_MAGIC 0xADA5A54B
struct cache cache_cache;
mutex_t cache_lock;
struct hash_table cache_hash, address_hash;
struct valloc slabs_reg;

static struct slab *allocate_new_slab(struct cache *cache)
{
	struct valloc_region reg;
	valloc_allocate(&slabs_reg, &reg, 1);
	for(addr_t a = reg.start; a < reg.start + SLAB_SIZE; a += PAGE_SIZE) {
		map_if_not_mapped(a);
	}
	struct slab *slab = (void *)reg.start;
	size_t slab_header_size = sizeof(struct slab);
	/* HACK ... */
	if(cache->object_size == PAGE_SIZE)
		slab_header_size = PAGE_SIZE;
	valloc_create(&slab->allocator, reg.start + slab_header_size, reg.start + reg.npages * SLAB_SIZE, cache->object_size, 0);
	slab->count = 0;
	slab->magic = SLAB_MAGIC;
	slab->max = (slab->allocator.npages - slab->allocator.nindex) - 1;
	slab->cache = cache;
	assert(slab->max > 2);

	return slab;
}

static inline struct slab *get_slab_from_object(void *object)
{
	return (struct slab *)(((addr_t)object) & ~(SLAB_SIZE - 1));
}

static void free_object(void *object)
{
	struct slab *slab = get_slab_from_object(object);
	assert(slab->magic == SLAB_MAGIC);
	assert(slab->count > 0 && slab->count <= slab->max);
	struct cache *cache = slab->cache;
	struct valloc_region reg;
	reg.start = (addr_t)object;
	reg.npages = 1;
	reg.flags = 0;
	valloc_deallocate(&slab->allocator, &reg);
	
	mutex_acquire(&cache->lock);
	if(slab->count == slab->max) {
		printk(0, "moving %x from full to partial\n", slab);
		ll_do_remove(&cache->full, &slab->node, 0);
		ll_do_insert(&cache->partial, &slab->node, slab);
	} else if(slab->count == 1) {
		printk(0, "moving %x from partial to empty\n", slab);
		ll_do_remove(&cache->partial, &slab->node, 0);
		ll_do_insert(&cache->empty, &slab->node, slab);
	}
	sub_atomic(&slab->count, 1);
	mutex_release(&cache->lock);
}

static void *allocate_object(struct slab *slab)
{
	struct valloc_region reg;
	/* printk(0, "alloc from %x: %d / %d\n", slab, slab->count, slab->max); */
	assert(valloc_allocate(&slab->allocator, &reg, 1));
	//printk(0, ":: %x %d\n", reg.start, reg.npages);
	return (void *)(reg.start);
}

static void *allocate_object_from_cache(struct cache *cache)
{
	mutex_acquire(&cache->lock);
	struct slab *slab;
	if(cache->partial.num > 0) {
		slab = ll_entry(struct slab *, cache->partial.head);
		if(slab->count == slab->max-1) {
		printk(0, "moving %x from partial to full\n", slab);
			ll_do_remove(&cache->partial, &slab->node, 0);
			ll_do_insert(&cache->full, &slab->node, slab);
		}
	} else if(cache->empty.num > 0) {
		slab = ll_entry(struct slab *, cache->empty.head);
		printk(0, "moving %x from empty to partial\n", slab);
		ll_do_remove(&cache->empty, &slab->node, 0);
		ll_do_insert(&cache->partial, &slab->node, slab);
	} else {
		//printk(0, "gotta make new slab\n");
		slab = allocate_new_slab(cache);
		ll_do_insert(&cache->partial, &slab->node, slab);
	}
	assert(slab->magic = SLAB_MAGIC);
	add_atomic(&slab->count, 1);
	mutex_release(&cache->lock);
	return allocate_object(slab);
}

static void construct_cache(struct cache *cache, size_t sz)
{
	ll_create_lockless(&cache->empty);
	ll_create_lockless(&cache->partial);
	ll_create_lockless(&cache->full);
	mutex_create(&cache->lock, MT_NOSCHED);
	cache->object_size = sz;
}

static struct cache *select_cache(size_t size)
{
	mutex_acquire(&cache_lock);
	struct cache *cache;
	if(hash_table_get_entry(&cache_hash, &size, sizeof(size), 1, (void **)&cache) == -ENOENT) {
		printk(0, "creating new cache for %d\n", size);
		size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
		assert(hash_table_get_entry(&cache_hash, &cachesize, sizeof(cachesize), 1, (void **)&cache) == 0);
		cache = allocate_object_from_cache(cache);
		construct_cache(cache, size);
		hash_table_set_entry(&cache_hash, &size, sizeof(size), 1, cache);
	}
	mutex_release(&cache_lock);
	return cache;
}

#define NUM_ENTRIES 128
static void *__entries[NUM_ENTRIES];

void slab_init(addr_t start, addr_t end)
{
	/* init the hash table */
	hash_table_create(&cache_hash, HASH_NOLOCK, HASH_TYPE_LINEAR);
	hash_table_specify_function(&cache_hash, HASH_FUNCTION_DEFAULT);
	cache_hash.entries = __entries;
	cache_hash.size = NUM_ENTRIES;

	/* init the cache_cache */
	construct_cache(&cache_cache, sizeof(struct cache));
	size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
	hash_table_set_entry(&cache_hash, &cachesize, sizeof(cachesize), 1, &cache_cache);
	mutex_create(&cache_lock, MT_NOSCHED);

	/* init the region */
	valloc_create(&slabs_reg, start, end, SLAB_SIZE, 0);

#warning "make this linear probing"
	hash_table_create(&address_hash, 0, HASH_TYPE_CHAIN);
	hash_table_specify_function(&address_hash, HASH_FUNCTION_DEFAULT);
	hash_table_resize(&address_hash, HASH_RESIZE_MODE_IGNORE, 1000);
}

void *slab_kmalloc(size_t size)
{
	size = ((size-1) & ~(63)) + 64;
	void *obj;
	if(size < SLAB_SIZE / 5) {
		struct cache *cache = select_cache(size);
		obj = allocate_object_from_cache(cache);
	} else {
		struct valloc_region reg;
		valloc_allocate(&slabs_reg, &reg, ((size-1) / SLAB_SIZE) + 1);
		for(addr_t a = reg.start; a < reg.start + reg.npages * SLAB_SIZE; a+=PAGE_SIZE)
			map_if_not_mapped(a);

		hash_table_set_entry(&address_hash, &reg.start, sizeof(addr_t), 1,
				(void *)(reg.start + reg.npages * SLAB_SIZE));
		obj = (void *)reg.start;
	}
	return obj;
}

void slab_kfree(void *data)
{
	void *ent;
	if(hash_table_get_entry(&address_hash, &data, sizeof(addr_t), 1, &ent) == 0) {
		struct valloc_region reg;
		reg.start = (addr_t)data;
		reg.npages = ((addr_t)ent - reg.start) / SLAB_SIZE;
		reg.flags = 0;
		valloc_deallocate(&slabs_reg, &reg);
		hash_table_delete_entry(&address_hash, &data, sizeof(addr_t), 1);
	} else {
		free_object(data);
	}
}

