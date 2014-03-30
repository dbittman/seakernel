#include <kernel.h>
#include <sea/lib/hash.h>
#include <types.h>


int hash_chain_get(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void **value)
{
	assert(fn && value);
	int loc = fn(size, key, elem_sz, len, 0);
	struct hash_table_chain_node *n = h[loc];
	while(n && __hash_table_compare_keys(n->key, n->elem_sz, n->len, key, elem_sz, len))
		n = n->next;
	if(!n) return -ENOENT;
	*value = n->entry;
	return 0;
}

int hash_chain_set(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value)
{
	assert(fn && value);
	int loc = fn(size, key, elem_sz, len, 0);
	struct hash_table_chain_node *n = h[loc], *prev=0;
	while(n) {
		if(!__hash_table_compare_keys(n->key, n->elem_sz, n->len, key, elem_sz, len))
			return -EEXIST;
		prev = n;
		n = n->next;
	}
	n = kmalloc(sizeof(struct hash_table_chain_node));
	n->key = kmalloc(elem_sz * len);
	memcpy(n->key, key, elem_sz * len);
	n->elem_sz = elem_sz;
	n->len = len;
	n->entry = value;
	
	if(!prev)
		h[loc] = n;
	else
		prev->next = n;
	struct hash_table_chain_node *head = h[loc];
	head->num_in_chain++;
	return 0;
}

int hash_chain_del(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len)
{
	assert(fn);
	int loc = fn(size, key, elem_sz, len, 0);
	struct hash_table_chain_node *n = h[loc], *prev=0;
	while(n && __hash_table_compare_keys(n->key, n->elem_sz, n->len, key, elem_sz, len)) {
		prev = n;
		n = n->next;
	}
	if(!n)
		return -ENOENT;
	if(prev) {
		prev->next = n->next;
		struct hash_table_chain_node *head = h[loc];
		head->num_in_chain--;
	}
	else {
		h[loc] = n->next;
		if(n->next)
			n->next->num_in_chain = n->num_in_chain-1;
	}
	kfree(n->key);
	kfree(n);
	return 0;
}

int hash_chain_enumerate(void **h, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value)
{
	size_t i=0;
	while(i < size) {
		struct hash_table_chain_node *n = h[i];
		if(n && n->num_in_chain > num) {
			while(n && num--)
				n=n->next;
			assert(n);
			if(key)
				*key = n->key;
			if(elem_sz)
				*elem_sz = n->elem_sz;
			if(len)
				*len = n->len;
			if(value)
				*value = n->entry;
			return 0;
		} else {
			if(n) 
				num -= n->num_in_chain;
			i++;
		}
	}
	return -ENOENT;
}

struct hash_collision_resolver __hash_chain_resolver = {
	"chain",
	hash_chain_get,
	hash_chain_set,
	hash_chain_del,
	hash_chain_enumerate
};
