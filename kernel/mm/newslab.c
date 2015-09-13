#include <sea/types.h>
#include <sea/ll.h>
#include <sea/mutex.h>
#include <sea/mm/valloc.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <stdatomic.h>
#include <sea/fs/kerfs.h>
struct slab {
	struct cache *cache;
	struct llistnode node;
	struct valloc allocator;
	_Atomic int count;
	int max;
	uint32_t magic;
	mutex_t lock;
};

struct cache {
	struct llist empty, partial, full;
	size_t slabcount;
	size_t object_size;
	mutex_t lock;
};

#define SLAB_SIZE 0x40000
#define SLAB_MAGIC 0xADA5A54B
struct cache cache_cache;
mutex_t cache_lock;
struct hash_table cache_hash;
struct valloc slabs_reg;

int full_slabs_count=0, partial_slabs_count=0, empty_slabs_count=0;
size_t total_allocated=0;

int slab_get_usage(void)
{
	return (full_slabs_count * 100 + partial_slabs_count * 50) / slabs_reg.npages;
}

int kerfs_kmalloc_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"Region Usage: %d / %d, Slab Usage: %d %d %d, cache hash load: %d%%\nTotal bytes allocated: %d\n",
			valloc_count_used(&slabs_reg), slabs_reg.npages,
			full_slabs_count, partial_slabs_count, empty_slabs_count,
			(cache_hash.count * 100) / cache_hash.size, total_allocated);
	void *ent;
	uint64_t n=0;
	while(hash_table_enumerate_entries(&cache_hash, n++, 0, 0, 0, &ent) != -ENOENT) {
		struct cache *cache = ent;
		KERFS_PRINTF(offset, length, buf, current,
				"cache %d: size=%d, slabcount=%d\n", (int)n, cache->object_size, cache->slabcount);
	}
	return current;
}

static struct slab *allocate_new_slab(struct cache *cache)
{
	struct valloc_region reg;
	if(valloc_allocate(&slabs_reg, &reg, 1) == 0) {
		panic(PANIC_NOSYNC, "could not allocate new slab");
	}
	mm_virtual_trymap(reg.start, PAGE_PRESENT | PAGE_WRITE, mm_page_size(0));
	mm_virtual_trymap(reg.start + mm_page_size(0), PAGE_PRESENT | PAGE_WRITE, mm_page_size(0));
	memset((void *)reg.start, 0, PAGE_SIZE * 2);
	struct slab *slab = (void *)reg.start;
	size_t slab_header_size = sizeof(struct slab);
	valloc_create(&slab->allocator, reg.start + slab_header_size,
			reg.start + reg.npages * SLAB_SIZE, cache->object_size, 0);
	slab->count = 0;
	slab->magic = SLAB_MAGIC;
	slab->max = (slab->allocator.npages - slab->allocator.nindex);
	slab->cache = cache;
	mutex_create(&slab->lock, MT_NOSCHED);
	cache->slabcount++;
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
	atomic_fetch_sub_explicit(&total_allocated, slab->cache->object_size, memory_order_relaxed);
	
	mutex_acquire(&cache->lock);
	int count = atomic_fetch_sub(&slab->count, 1);
	if(count == slab->max) {
		atomic_fetch_sub_explicit(&full_slabs_count, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		ll_do_remove(&cache->full, &slab->node, 0);
		ll_do_insert(&cache->partial, &slab->node, slab);
	} else if(count == 1) {
		atomic_fetch_sub_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&empty_slabs_count, 1, memory_order_relaxed);
		ll_do_remove(&cache->partial, &slab->node, 0);
		ll_do_insert(&cache->empty, &slab->node, slab);
	}
	mutex_release(&cache->lock);
}

static void *allocate_object(struct slab *slab)
{
	struct valloc_region reg;
	void *test = valloc_allocate(&slab->allocator, &reg, 1);
	assertmsg(test, "could not allocate object from valloc in slab");
	
	/* NOTE: do this while we're still locked, because objects can share physical pages,
	 * so if two processes try to map a page at the same time, sadness can happpen. */
	/* mutex_acquire(&slab->lock); */
	/* TODO: either this, or atomic mapping */
	for(addr_t a = reg.start;a < reg.start + slab->cache->object_size + PAGE_SIZE;a+=PAGE_SIZE) {
		/* TODO: page size? */
		mm_virtual_trymap(a, PAGE_PRESENT | PAGE_WRITE, PAGE_SIZE);
	}
	/* mutex_release(&slab->lock); */
	atomic_fetch_add_explicit(&total_allocated, slab->cache->object_size, memory_order_relaxed);
	return (void *)(reg.start);
}

static void *allocate_object_from_cache(struct cache *cache)
{
	mutex_acquire(&cache->lock);
	struct slab *slab;
	if(cache->partial.num > 0) {
		slab = ll_entry(struct slab *, cache->partial.head);
		if(slab->count == slab->max-1) {
			atomic_fetch_sub_explicit(&partial_slabs_count, 1, memory_order_relaxed);
			atomic_fetch_add_explicit(&full_slabs_count, 1, memory_order_relaxed);
			ll_do_remove(&cache->partial, &slab->node, 0);
			ll_do_insert(&cache->full, &slab->node, slab);
		}
	} else if(cache->empty.num > 0) {
		atomic_fetch_sub_explicit(&empty_slabs_count, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		slab = ll_entry(struct slab *, cache->empty.head);
		ll_do_remove(&cache->empty, &slab->node, 0);
		ll_do_insert(&cache->partial, &slab->node, slab);
	} else {
		slab = allocate_new_slab(cache);
		atomic_fetch_add_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		ll_do_insert(&cache->partial, &slab->node, slab);
	}
	assert(slab->magic = SLAB_MAGIC);
	atomic_fetch_add(&slab->count, 1);
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
	cache->slabcount=0;
}

static struct cache *select_cache(size_t size)
{
	mutex_acquire(&cache_lock);
	struct cache *cache;
	if(hash_table_get_entry(&cache_hash, &size, sizeof(size), 1, (void **)&cache) == -ENOENT) {
		size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
		int v = hash_table_get_entry(&cache_hash, &cachesize, sizeof(cachesize), 1, (void **)&cache);
		assert(!v);
		cache = allocate_object_from_cache(cache);
		construct_cache(cache, size);
		hash_table_set_entry(&cache_hash, &cache->object_size, sizeof(cache->object_size), 1, cache);
	}
	mutex_release(&cache_lock);
	return cache;
}

#define NUM_ENTRIES 256
static struct hash_linear_entry __entries[NUM_ENTRIES];
void slab_init(addr_t start, addr_t end)
{
	/* init the hash table */
	hash_table_create(&cache_hash, HASH_NOLOCK, HASH_TYPE_LINEAR);
	hash_table_specify_function(&cache_hash, HASH_FUNCTION_DEFAULT);
	memset(__entries, 0, sizeof(__entries));
	cache_hash.entries = (void **)__entries;
	cache_hash.size = NUM_ENTRIES;

	/* init the cache_cache */
	size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
	construct_cache(&cache_cache, cachesize);
	hash_table_set_entry(&cache_hash, &cache_cache.object_size, sizeof(cache_cache.object_size), 1, &cache_cache);
	mutex_create(&cache_lock, MT_NOSCHED);

	/* init the region */
	valloc_create(&slabs_reg, start, end, SLAB_SIZE, 0);
}

#define CANARY 0

void *slab_kmalloc(size_t __size)
{
	assert(__size);
#if CANARY
	size_t size = (((__size + sizeof(uint32_t)*2 + sizeof(size_t))-1) & ~(63)) + 64;
#else
	size_t size = (__size & ~(63)) + 64;
#endif
	if(size >= 0x1000) {
		size = ((__size - 1) & ~(0x1000 - 1)) + 0x1000;
	}
	assert(size >= __size);
	void *obj = 0;
	if(size <= SLAB_SIZE / 4) {
		struct cache *cache = select_cache(size);
		obj = allocate_object_from_cache(cache);

	} else {
		panic(PANIC_NOSYNC, "cannot allocate things that big (%d)!", size);
	}
#if CANARY
	uint32_t *canary = (uint32_t *)(obj);
	uint32_t *canary2 = (uint32_t *)((addr_t)obj + __size + sizeof(uint32_t) + sizeof(size_t));
	size_t *sz = (size_t *)((addr_t)obj + sizeof(*canary));
	*sz = __size;
	obj = (void *)((addr_t)obj + sizeof(*canary) + sizeof(size_t));
	assert(*canary != 0x5a5a6b6b);
	assert(*canary2 != 0x5a5a7c7c);
	*canary = 0x5a5a6b6b;
	*canary2 = 0x5a5a7c7c;
#endif
	return obj;
}

void slab_kfree(void *data)
{
#if CANARY
	data = (void *)((addr_t)data - (sizeof(uint32_t) + sizeof(size_t)) );
	uint32_t *canary = data;
	size_t sz = *(size_t *)((addr_t)data + sizeof(*canary));
	uint32_t *canary2 = (uint32_t *)((addr_t)data + sizeof(uint32_t) + sz + sizeof(size_t));
	
	assert(*canary2 == 0x5a5a7c7c);
	assert(*canary == 0x5a5a6b6b);
	memset(data, 0x4A, sz + sizeof(uint32_t) + sizeof(size_t));
	*canary = 0;
	*canary2 = 0;
#endif
	free_object(data);
}

