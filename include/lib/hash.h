#ifndef __SEA_LIB_HASH
#define __SEA_LIB_HASH

#include <types.h>

struct hash_table {
	unsigned flags;
	uint32_t magic;
	int size;
	uint32_t *entries;
	struct hash_collision_resolver *resolver;
};

struct hash_collision_resolver {
	char *name;
	void * (*get_entry)(struct hash_table *h, void *key, size_t elem_sz, size_t len);
	void (*set_entry)(struct hash_table *h, void *key, size_t elem_sz, size_t len, void *value);
	void * (*enumerate)(struct hash_table *h, uint64_t num);
};


#define HASH_MAGIC 0xABCD1234

#define HASH_ALLOC     1

#define HASH_RESIZE_MODE_REHASH 1
#define HASH_RESIZE_MODE_DELETE 2

#define HASH_TYPE_NONE      0
#define HASH_TYPE_CHAIN     1
#define HASH_TYPE_LINEAR    2
#define HASH_TYPE_QUAD      3
#define HASH_TYPE_DOUBLE    4

#define NUM_HASH_COLLISION_RESOLVERS 5

#endif
