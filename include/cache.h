#ifndef CACHE_H
#define CACHE_H

#include <rwlock.h>
#include <ll.h>
#include <lib/hash.h>

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

int destroy_all_id(cache_t *c, uint64_t);
int do_cache_object(cache_t *, uint64_t id, uint64_t key, int sz, char *buf, int dirty);
cache_t * get_empty_cache(int (*)(struct ce_t *), char *);
struct ce_t *find_cache_element(cache_t *, uint64_t id, uint64_t key);
void sync_cache(cache_t *);
int destroy_cache(cache_t *);
int sync_element(cache_t *, struct ce_t *e);
void remove_element(cache_t *, struct ce_t *o, int);
void do_sync_of_mounted();
int kernel_cache_sync();
int do_sync_element(cache_t *c, struct ce_t *e, int locked);
void sync_dm();
#endif
