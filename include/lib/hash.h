#ifndef __SEA_LIB_HASH
#define __SEA_LIB_HASH

#include <types.h>
#include <rwlock.h>

struct hash_table {
	unsigned flags;
	uint32_t magic;
	int size;
	void **entries;
	struct hash_collision_resolver *resolver;
	rwlock_t lock;
};

struct hash_collision_resolver {
	char *name;
	int (*get)(void **h, size_t size, void *key, size_t elem_sz, size_t len, void **value);
	int (*set)(void **h, size_t size, void *key, size_t elem_sz, size_t len, void *value);
	int (*del)(void **h, size_t size, void *key, size_t elem_sz, size_t len);
	int (*enumerate)(void **h, size_t size, uint64_t num, void **key, size_t *elem_sz, size_t *len, void **value);
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
