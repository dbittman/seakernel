#include <sea/kernel.h>
#include <sea/lib/hash.h>
#include <sea/types.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

struct entry {
	void *key;
	uint64_t __key;
	size_t len;
	size_t elem_sz;
	void *data;
};

int hash_linear_get(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void **value)
{
	assert(fn);
	size_t loc = fn(size, key, elem_sz, len, 0);
	size_t start = loc;
	struct entry *entry;
	while((entry = &((struct entry *)h)[loc]) && __hash_table_compare_keys(entry->key, entry->elem_sz, entry->len, key, elem_sz, len)) {
		loc++;
		if(loc >= size)
			loc = 0;
		if(loc == start || !entry->data) {
			return -ENOENT;
		}
	}
	if(value)
		*value = entry->data;
	return 0;
}

int hash_linear_set_or_get(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value, void **result)
{
		panic(0, "not implemented");
}

int hash_linear_set(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value)
{
	assert(fn && value);
	size_t loc = fn(size, key, elem_sz, len, 0);
	size_t start = loc;

	struct entry *entry;
	while((entry = &((struct entry *)h)[loc]) && entry->data) {
		if(!__hash_table_compare_keys(entry->key, entry->elem_sz, entry->len, key, elem_sz, len))
			return -EEXIST;
		loc++;
		if(loc == size)
			loc = 0;
		if(loc == start)
			panic(0, "hash table full");
	}
	
	if(elem_sz <= sizeof(entry->__key) && len == 1) {
		entry->__key = *(size_t *)key;
		entry->key = &entry->__key;
		entry->elem_sz = elem_sz;
		entry->len = len;
		entry->data = value;
	} else {
		panic(0, "not implemented");
	}
	return 0;
}

int hash_linear_del(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len)
{
		panic(0, "not implemented");
}

int hash_linear_enumerate(void **h, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value)
{
		panic(0, "not implemented");
}

struct hash_collision_resolver __hash_linear_resolver = {
	"linear-probe",
	sizeof(struct entry),
	hash_linear_get,
	hash_linear_set,
	hash_linear_set_or_get,
	hash_linear_del,
	hash_linear_enumerate
};

