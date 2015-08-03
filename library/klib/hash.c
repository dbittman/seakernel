#include <sea/kernel.h>
#include <sea/lib/hash.h>
#include <sea/mm/vmm.h>
#include <sea/types.h>
#include <sea/rwlock.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

static struct hash_collision_resolver *hash_collision_resolvers[NUM_HASH_COLLISION_RESOLVERS] = {
	0,
	&__hash_chain_resolver,
	&__hash_linear_resolver,
	0,
	0
};

int __default_byte_sum_fn(int sz, void *key, size_t kesz, size_t len, int iteration)
{
	unsigned int i;
	unsigned sum=0;
	unsigned char tmp;
	for(i=0;i<len*kesz;i++)
	{
		tmp = *(((unsigned char *)key) + i);
		sum += tmp;
	}
	sum += iteration;
	return sum % sz;
}

int __hash_fn_djb2(int sz, void *key, size_t kesz, size_t len, int iteration)
{
	unsigned long hash = 5381;
	unsigned char *buf = key;
	for(int i = 0;i < kesz*len;i++) {
		unsigned char e = buf[i];
		hash = ((hash << 5) + hash) + e;
	}
	hash += iteration;
	return hash % sz;
}

#warning "redesign hashing system to not need to do allocations beyond The Big Array"
static void *hash_functions_list[NUM_HASH_FUNCTIONS] = {
	__default_byte_sum_fn,
	__hash_fn_djb2
};

int __hash_table_compare_keys(void *key_1, size_t es_1, size_t len_1, void *key_2, size_t es_2, size_t len_2)
{
	if(es_1 != es_2) return (int)(es_1 - es_2);
	if(len_1 != len_2) return (int)(len_1 - len_2);
	return memcmp(key_1, key_2, es_1*len_1);
}

static int __hash_table_enumerate(struct hash_table *h, void **e, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value)
{
	return h->resolver->enumerate(e, size, num, key, elem_sz, len, value);
}

static int __hash_table_get(struct hash_table *h, void **e, size_t size, void *key, size_t elem_sz, size_t len, void **value)
{
	return h->resolver->get(e, h->fn, size, key, elem_sz, len, value);
}

static int __hash_table_set_or_get(struct hash_table *h, void **e, size_t size, void *key, size_t elem_sz, size_t len, void *value, void **result)
{
	return h->resolver->set_or_get(e, h->fn, size, key, elem_sz, len, value, result);
}

static int __hash_table_set(struct hash_table *h, void **e, size_t size, void *key, size_t elem_sz, size_t len, void *value)
{
	return h->resolver->set(e, h->fn, size, key, elem_sz, len, value);
}

static int __hash_table_delete(struct hash_table *h, void **e, size_t size, void *key, size_t elem_sz, size_t len)
{
	return h->resolver->del(e, h->fn, size, key, elem_sz, len);
}

struct hash_table *hash_table_create(struct hash_table *h, unsigned flags, unsigned type)
{
	if(!h) {
		h = kmalloc(sizeof(struct hash_table));
		h->flags |= (HASH_ALLOC | flags);
	} else
		h->flags = flags;
	assert(type < NUM_HASH_COLLISION_RESOLVERS);
	h->resolver = hash_collision_resolvers[type];
	if(!h->resolver)
		panic(0, "hash table collision resolver %d requested but not implemented!", type);
	h->size = 0;
	h->entries = 0;
	h->count=0;
	h->fn = 0;
	if(!(h->flags & HASH_NOLOCK))
		rwlock_create(&h->lock);
	h->magic = HASH_MAGIC;
	return h;
}

void hash_table_specify_function(struct hash_table *h, unsigned fn)
{
	assert(fn < NUM_HASH_FUNCTIONS);
	assert(h && h->magic == HASH_MAGIC);
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_WRITER);
	assert(!h->fn || (h->count==0));
	h->fn = hash_functions_list[fn];
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_WRITER);
}

int hash_table_resize(struct hash_table *h, unsigned mode, size_t new_size)
{
	if(!h || h->magic != HASH_MAGIC) panic(0, "hash resize on invalid hash table");
	int old_size = h->size;

	void **ne = kmalloc(h->resolver->entrysz * new_size);
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_WRITER);
	if(h->entries) {
		if(mode == HASH_RESIZE_MODE_REHASH) {
			uint64_t i = 0;
			void *key, *value;
			size_t elem_sz, len;
			while(__hash_table_enumerate(h, h->entries, h->size, i++, &key, &elem_sz, &len, &value) != -ENOENT) {
				__hash_table_set(h, ne, new_size, key, elem_sz, len, value);
				__hash_table_delete(h, h->entries, h->size, key, elem_sz, len);
			}
		} else if(mode == HASH_RESIZE_MODE_DELETE) {
			uint64_t i = 0;
			void *key, *value;
			size_t elem_sz, len;
			while(__hash_table_enumerate(h, h->entries, h->size, i++, &key, &elem_sz, &len, &value) != -ENOENT)
				__hash_table_delete(h, h->entries, h->size, key, elem_sz, len);
			h->count=0;
		} else if(mode != HASH_RESIZE_MODE_IGNORE)
			panic(0, "hash resize got invalid mode %d", mode);
		kfree(h->entries);
	}

	h->size = new_size;
	h->entries = ne;

	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_WRITER);

	return old_size;
}

int hash_table_get_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len, void **value)
{
	if(!h || !key || !h->entries || h->magic != HASH_MAGIC) panic(0, "invalid hash table for hash_table_get_entry");
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_READER);
	int ret = __hash_table_get(h, h->entries, h->size, key, key_element_size, key_len, value);
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_READER);
	return ret;
}

int hash_table_set_or_get_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len, void *value, void **result)
{
	if(!h || !key || !h->entries || h->magic != HASH_MAGIC) panic(0, "invalid hash table for hash_table_set_entry");
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_WRITER);
	int ret = __hash_table_set_or_get(h, h->entries, h->size, key, key_element_size, key_len, value, result);
	if(ret == 0)
		h->count++;
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_WRITER);
	return ret;
}


int hash_table_set_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len, void *value)
{
	if(!h || !key || !h->entries || h->magic != HASH_MAGIC) panic(0, "invalid hash table for hash_table_set_entry");
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_WRITER);
	int ret = __hash_table_set(h, h->entries, h->size, key, key_element_size, key_len, value);
	if(ret == 0)
		h->count++;
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_WRITER);
	return ret;
}

int hash_table_delete_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len)
{
	if(!h || !key || !h->entries || h->magic != HASH_MAGIC) panic(0, "invalid hash table for hash_table_delete_entry");
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_WRITER);
	int ret = __hash_table_delete(h, h->entries, h->size, key, key_element_size, key_len);
	if(ret == 0)
		h->count--;
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_WRITER);
	return ret;
}

int hash_table_enumerate_entries(struct hash_table *h, uint64_t num, void **key, size_t *key_element_size, size_t *key_len, void **value)
{
	if(!h || !h->entries || h->magic != HASH_MAGIC) panic(0, "invalid hash table for hash_table_enumerate_entries");
	if(!(h->flags & HASH_NOLOCK))
		rwlock_acquire(&h->lock, RWL_READER);
	int ret = __hash_table_enumerate(h, h->entries, h->size, num, key, key_element_size, key_len, value);
	if(!(h->flags & HASH_NOLOCK))
		rwlock_release(&h->lock, RWL_READER);
	return ret;
}

void hash_table_destroy(struct hash_table *h)
{
	if(!h || h->magic != HASH_MAGIC) panic(0, "tried to destroy invalid hash table");

	uint64_t i = 0;
	void *key;
	size_t elem_sz, len;
	rwlock_acquire(&h->lock, RWL_WRITER);
	while(__hash_table_enumerate(h, h->entries, h->size, i++, &key, &elem_sz, &len, 0) != -ENOENT)
		__hash_table_delete(h, h->entries, h->size, key, elem_sz, len);
	kfree(h->entries);
	if(!(h->flags & HASH_NOLOCK))
		rwlock_destroy(&h->lock);
	h->magic=0;
	if(h->flags & HASH_ALLOC)
		kfree(h);
}

