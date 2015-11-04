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
	struct linkedentry node;
	struct valloc allocator;
	_Atomic int count;
	int max;
	uint32_t magic;
	mutex_t lock;
};

struct cache {
	struct linkedlist empty, partial, full;
	size_t slabcount;
	size_t object_size;
	mutex_t lock;
	struct hashelem hash_elem;
};

#define SLAB_SIZE mm_page_size(1)
#define SLAB_MAGIC 0xADA5A54B
struct cache cache_cache;
mutex_t cache_lock;
struct hash cache_hash;
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
			(hash_count(&cache_hash) * 100) / hash_length(&cache_hash), total_allocated);
	return current;
}

static struct slab *allocate_new_slab(struct cache *cache)
{
	struct valloc_region reg;
	if(valloc_allocate(&slabs_reg, &reg, 1) == 0) {
		panic(PANIC_NOSYNC, "could not allocate new slab");
	}
	mm_virtual_trymap(reg.start, PAGE_PRESENT | PAGE_WRITE, SLAB_SIZE);
	memset((void *)reg.start, 0, sizeof(struct slab));
	struct slab *slab = (void *)reg.start;
	size_t slab_header_size = sizeof(struct slab);
	valloc_create(&slab->allocator, reg.start + slab_header_size,
			reg.start + reg.npages * SLAB_SIZE, cache->object_size, 0);
	slab->count = 0;
	slab->magic = SLAB_MAGIC;
	slab->max = (slab->allocator.npages - slab->allocator.nindex);
	slab->cache = cache;
	mutex_create(&slab->lock, 0);
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
		linkedlist_remove(&cache->full, &slab->node);
		linkedlist_insert(&cache->partial, &slab->node, slab);
	} else if(count == 1) {
		atomic_fetch_sub_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&empty_slabs_count, 1, memory_order_relaxed);
		linkedlist_remove(&cache->partial, &slab->node);
		linkedlist_insert(&cache->empty, &slab->node, slab);
	}
	mutex_release(&cache->lock);
}

static void *allocate_object(struct slab *slab)
{
	struct valloc_region reg;
	void *test = valloc_allocate(&slab->allocator, &reg, 1);
	assertmsg(test, "could not allocate object from valloc in slab");
	atomic_fetch_add_explicit(&total_allocated, slab->cache->object_size, memory_order_relaxed);
	return (void *)(reg.start);
}

static void *allocate_object_from_cache(struct cache *cache)
{
	mutex_acquire(&cache->lock);
	struct slab *slab;
	if(cache->partial.count > 0) {
		slab = linkedlist_head(&cache->partial);
		assert(slab);
		if(slab->count == slab->max-1) {
			atomic_fetch_sub_explicit(&partial_slabs_count, 1, memory_order_relaxed);
			atomic_fetch_add_explicit(&full_slabs_count, 1, memory_order_relaxed);
			linkedlist_remove(&cache->partial, &slab->node);
			linkedlist_insert(&cache->full, &slab->node, slab);
		}
	} else if(cache->empty.count > 0) {
		atomic_fetch_sub_explicit(&empty_slabs_count, 1, memory_order_relaxed);
		atomic_fetch_add_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		slab = linkedlist_head(&cache->empty);
		assert(slab);
		linkedlist_remove(&cache->empty, &slab->node);
		linkedlist_insert(&cache->partial, &slab->node, slab);
	} else {
		slab = allocate_new_slab(cache);
		atomic_fetch_add_explicit(&partial_slabs_count, 1, memory_order_relaxed);
		linkedlist_insert(&cache->partial, &slab->node, slab);
	}
	assert(slab->magic = SLAB_MAGIC);
	atomic_fetch_add(&slab->count, 1);
	mutex_release(&cache->lock);
	void *ret = allocate_object(slab);
	return ret;
}

static void construct_cache(struct cache *cache, size_t sz)
{
	linkedlist_create(&cache->empty, LINKEDLIST_LOCKLESS);
	linkedlist_create(&cache->partial, LINKEDLIST_LOCKLESS);
	linkedlist_create(&cache->full, LINKEDLIST_LOCKLESS);
	mutex_create(&cache->lock, 0);
	cache->object_size = sz;
	cache->slabcount=0;
}

static struct cache *select_cache(size_t size)
{
	mutex_acquire(&cache_lock);
	struct cache *cache;
	if((cache = hash_lookup(&cache_hash, &size, sizeof(size))) == NULL) {
		size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
		cache = hash_lookup(&cache_hash, &cachesize, sizeof(cachesize));
		assert(cache);
		cache = allocate_object_from_cache(cache);
		construct_cache(cache, size);
		hash_insert(&cache_hash, &cache->object_size, sizeof(cache->object_size), &cache->hash_elem, cache);
	}
	mutex_release(&cache_lock);
	return cache;
}

#define NUM_ENTRIES 256
static struct linkedlist *__entries[NUM_ENTRIES];
static struct linkedlist __entries_list[NUM_ENTRIES];
void slab_init(addr_t start, addr_t end)
{
	/* init the hash table */
	hash_create(&cache_hash, 0, 0);
	for(int i=0;i<NUM_ENTRIES;i++) {
		__entries[i] = &__entries_list[i];
		linkedlist_create(__entries[i], 0);
	}
	cache_hash.table = (struct linkedlist **)__entries;
	cache_hash.length = NUM_ENTRIES;

	/* init the cache_cache */
	size_t cachesize = ((sizeof(struct cache) - 1) & ~63) + 64;
	construct_cache(&cache_cache, cachesize);
	hash_insert(&cache_hash, &cache_cache.object_size, sizeof(cache_cache.object_size),
			&cache_cache.hash_elem, &cache_cache);
	mutex_create(&cache_lock, 0);

	/* init the region */
	valloc_create(&slabs_reg, start, end, SLAB_SIZE, 0);
}

#define CANARY 1

void *slab_kmalloc(size_t __size)
{
	assert(__size);
#if CANARY
	size_t size = (((__size + sizeof(uint32_t)*2 + sizeof(size_t))-1) & ~(63)) + 64;
#else
	size_t size = (__size & ~(63)) + 64;
#endif
	if(size >= 0x1000) {
		size = ((size - 1) & ~(0x1000 - 1)) + 0x1000;
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
	memset(obj, 0, __size);
	return obj;
}

void slab_kfree(void *data)
{
	assert((addr_t)data >= MEMMAP_KMALLOC_START && (addr_t)data < MEMMAP_KMALLOC_END);
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

