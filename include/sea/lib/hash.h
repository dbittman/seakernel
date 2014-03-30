#ifndef __SEA_LIB_HASH
#define __SEA_LIB_HASH

#include <sea/types.h>
#include <sea/rwlock.h>

struct hash_table {
	unsigned flags;
	uint32_t magic;
	int size, count;
	void **entries;
	int (*fn)(int table_size, void *key, size_t elem_sz, size_t len, int i);
	struct hash_collision_resolver *resolver;
	rwlock_t lock;
};

struct hash_collision_resolver {
	char *name;
	int (*get)(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void **value);
	int (*set)(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len, void *value);
	int (*del)(void **h, int (*fn)(int, void *, size_t, size_t, int), size_t size, void *key, size_t elem_sz, size_t len);
	int (*enumerate)(void **h, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value);
};


#define HASH_MAGIC 0xABCD1234

#define HASH_ALLOC     1

#define HASH_RESIZE_MODE_IGNORE 0
#define HASH_RESIZE_MODE_REHASH 1
#define HASH_RESIZE_MODE_DELETE 2

#define HASH_TYPE_NONE      0
#define HASH_TYPE_CHAIN     1
#define HASH_TYPE_LINEAR    2
#define HASH_TYPE_QUAD      3
#define HASH_TYPE_DOUBLE    4

#define HASH_FUNCTION_BYTE_SUM 0

#define NUM_HASH_COLLISION_RESOLVERS 5
#define NUM_HASH_FUNCTIONS           1

int __hash_table_compare_keys(void *key_1, size_t es_1, size_t len_1, void *key_2, size_t es_2, size_t len_2);

struct hash_table *hash_table_create(struct hash_table *h, unsigned flags, unsigned type);
int hash_table_resize(struct hash_table *h, unsigned mode, size_t new_size);
void hash_table_specify_function(struct hash_table *h, unsigned fn);
int hash_table_get_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len, void **value);
int hash_table_set_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len, void *value);
int hash_table_delete_entry(struct hash_table *h, void *key, size_t key_element_size, size_t key_len);
int hash_table_enumerate_entries(struct hash_table *h, uint64_t num, void **key, size_t *key_element_size, size_t *key_len, void **value);
void hash_table_destroy(struct hash_table *h);

/* hash_chain */

struct hash_table_chain_node {
	void *entry;
	void *key;
	size_t elem_sz, len;
	size_t num_in_chain;
	struct hash_table_chain_node *next;
};

extern struct hash_collision_resolver __hash_chain_resolver;

#endif
