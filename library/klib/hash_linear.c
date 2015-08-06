#include <sea/kernel.h>
#include <sea/lib/hash.h>
#include <sea/types.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

int hash_linear_get(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void **value)
{
	assert(fn);
	size_t loc = fn(size, key, elem_sz, len, 0);
	size_t start = loc;
	struct hash_linear_entry *entry;
	struct hash_linear_entry *entries = (struct hash_linear_entry *)h;
	while((entry = &entries[loc]) && entry->data && __hash_table_compare_keys(entry->key, entry->elem_sz, entry->len, key, elem_sz, len)) {
		loc++;
		if(loc >= size)
			loc = 0;
		if(loc == start || !entry->data) {
			return -ENOENT;
		}
	}
	if(!entry->data)
		return -ENOENT;
	if(value)
		*value = entry->data;
	return 0;
}

int hash_linear_set_or_get(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value, void **result)
{
	panic(0, "not implemented");
	return 0;
}

int hash_linear_set(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value)
{
	assert(fn && value);
	size_t loc = fn(size, key, elem_sz, len, 0);
	size_t start = loc;

	struct hash_linear_entry *entry;
	struct hash_linear_entry *entries = (struct hash_linear_entry *)h;
	while((entry = &entries[loc]) && entry->data) {
		if(!__hash_table_compare_keys(entry->key, entry->elem_sz, entry->len, key, elem_sz, len))
			return -EEXIST;
		loc++;
		if(loc == size)
			loc = 0;
		if(loc == start)
			panic(PANIC_NOSYNC, "hash table full");
	}
	assert(!entry->data && !entry->key);
	entry->key = key;
	entry->elem_sz = elem_sz;
	entry->len = len;
	entry->data = value;
	return 0;
}

int hash_linear_del(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len)
{
	size_t loc = fn(size, key, elem_sz, len, 0);
	size_t start = loc;
	struct hash_linear_entry *entry;
	struct hash_linear_entry *entries = (struct hash_linear_entry *)h;
	while((entry = &entries[loc]) && __hash_table_compare_keys(entry->key, entry->elem_sz, entry->len, key, elem_sz, len)) {
		loc++;
		if(loc >= size)
			loc = 0;
		if(loc == start || !entry->data) {
			return -ENOENT;
		}
	}
	entry->data = entry->key = 0;
	return 0;
}

int hash_linear_enumerate(void **h, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value)
{
	size_t loc = 0;
	struct hash_linear_entry *entry;
	struct hash_linear_entry *entries = (struct hash_linear_entry *)h;
	while((entry = &entries[loc])) {
		if(entry->data) {
			if(!num--)
				break;
		}
		loc++;
		if(loc >= size) {
			return -ENOENT;
		}
	}

	if(key)
		*key = entry->key;
	if(elem_sz)
		*elem_sz = entry->elem_sz;
	if(len)
		*len = entry->len;
	if(value)
		*value = entry->data;

	return 0;
}

struct hash_collision_resolver __hash_linear_resolver = {
	"linear-probe",
	sizeof(struct hash_linear_entry),
	hash_linear_get,
	hash_linear_set,
	hash_linear_set_or_get,
	hash_linear_del,
	hash_linear_enumerate
};

