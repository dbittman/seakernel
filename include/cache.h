#ifndef CACHE_H
#define CACHE_H

typedef struct chash_chain_s {
	void *ptr;
	unsigned id, key;
	struct chash_chain_s *next, *prev;
} chash_chain_t;

typedef struct {
	unsigned length;
	chash_chain_t **hash;
} chash_t;

struct ce_t {
	unsigned id, key;
	char dirty;
	char *data;
	unsigned length;
	unsigned atime;
	unsigned acount;
	unsigned dev;
	struct ce_t *next_dirty, *next;
	struct ce_t *prev_dirty, *prev;
};

typedef struct {
	unsigned dirty;
	unsigned count, acc, slow, syncing;
	char flag;
	chash_t *hash;
	int (*sync)(struct ce_t *);
	mutex_t lock, dlock;
	struct ce_t *dlist, *list, *tail; 
} cache_t;

extern cache_t caches[NUM_CACHES];

#define cache_object(c,id,key,sz,buf) do_cache_object(c, id, key, sz, buf, 1)
#define cache_object_clean(c,id,key,sz,buf) do_cache_object(c, id, key, sz, buf, 0)

chash_t *chash_create(unsigned length);
int chash_destroy(chash_t *h);
void *chash_search(chash_t *h, unsigned id, unsigned key);
int chash_delete(chash_t *h, unsigned id, unsigned key);
int chash_add(chash_t *h, unsigned id, unsigned key, void *ptr);

int do_cache_object(int c, unsigned id, unsigned key, int sz, char *buf, int dirty);
int get_empty_cache(int (*)(struct ce_t *));
struct ce_t *find_cache_element(int c, unsigned id, unsigned key);
void sync_cache(int id, int red, int slow, int rm);
int destroy_cache(int id, int);
int sync_element(int c, struct ce_t *e);
void remove_element(int c, struct ce_t *o);
void do_sync_of_mounted();
int kernel_cache_sync(int all, int);
int kernel_cache_sync_slow(int all);
void sync_dm();
#endif
