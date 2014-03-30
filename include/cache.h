#ifndef CACHE_H
#define CACHE_H

#include <sea/rwlock.h>
#include <sea/ll.h>
#include <sea/lib/hash.h>

struct ce_t {
	uint64_t id, key;
	char dirty;
	char *data;
	unsigned length;
	unsigned atime;
	unsigned acount;
	dev_t dev;
	rwlock_t *rwl;
	struct llistnode *dirty_node, *list_node;
};

typedef struct cache_t_s {
	unsigned dirty;
	unsigned count, acc, slow, syncing;
	struct hash_table *hash;
	int (*sync)(struct ce_t *);
	rwlock_t *rwl;
	char name[32];
	struct llist dirty_ll, primary_ll;
	struct cache_t_s *next, *prev;
} cache_t;

extern cache_t caches[NUM_CACHES];

#define cache_object(c,id,key,sz,buf) do_cache_object(c, id, key, sz, buf, 1)
#define cache_object_clean(c,id,key,sz,buf) do_cache_object(c, id, key, sz, buf, 0)

int cache_destroy_all_id(cache_t *c, uint64_t);
int do_cache_object(cache_t *, uint64_t id, uint64_t key, int sz, char *buf, int dirty);
cache_t * cache_create(int (*)(struct ce_t *), char *);
struct ce_t *cache_find_element(cache_t *, uint64_t id, uint64_t key);
void cache_sync(cache_t *);
int cache_destroy(cache_t *);
int cache_sync_element(cache_t *, struct ce_t *e);
void cache_remove_element(cache_t *, struct ce_t *o, int);
int cache_sync_all();

#endif
