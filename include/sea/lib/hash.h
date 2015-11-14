#ifndef __SEA_LIB_HASH
#define __SEA_LIB_HASH

#include <stdint.h>
#include <sea/types.h>
#include <sea/mutex.h>
#include <sea/lib/linkedlist.h>

#define HASH_ALLOC 1
#define HASH_LOCKLESS 2

struct hashelem {
	void *ptr;
	const void *key;
	size_t keylen;
	struct linkedentry entry;
};

struct hash {
	struct linkedlist **table;
	size_t length, count;
	int flags;
	struct mutex lock;
};

static inline size_t hash_count(struct hash *h) { return h->count; }
static inline size_t hash_length(struct hash *h) { return h->length; }

struct hash *hash_create(struct hash *h, int flags, size_t length);
void hash_destroy(struct hash *h);
int hash_insert(struct hash *h, const void *key, size_t keylen, struct hashelem *elem, void *data);
int hash_delete(struct hash *h, const void *key, size_t keylen);
void *hash_lookup(struct hash *h, const void *key, size_t keylen);
void hash_map(struct hash *h, void (*fn)(struct hashelem *obj));
void hash_map_data(struct hash *h, void (*fn)(struct hashelem *obj), void *data);

#endif
