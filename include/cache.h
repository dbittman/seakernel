#ifndef CACHE_H
#define CACHE_H

#include <btree.h>
struct ce_t {
	char flag;
	unsigned key;
	char busy;
	char dirty;
	char *data;
	unsigned length;
	unsigned atime;
	unsigned acount;
	char name[32];
	
	struct ce_t *next_dirty;
	struct ce_t *prev_dirty;
	struct ce_t *next_rl, *t_next;
	struct ce_t *prev_rl, *t_prev;
};


typedef struct {
	unsigned dirty;
	char pad1[4];
	unsigned count;
	unsigned nrobj;
	char pad2[4];
	unsigned int syncing;
	char flag;
	int (*sync)(struct ce_t *);
	int (*sync_multiple)(int, struct ce_t *, char *, int);
	bptree *bt[NUM_TREES];
	unsigned slow, acc, l_rc;
	mutex_t lock, dlock;
	struct ce_t *dlist, *rlist; /* Double linked-list of dirty items. 
	We can use a DLL because we never have to search this list. 
	All operations approach O(1). Which is pretty cool.
	*/
	struct ce_t *last;
} cache_t;

struct cache_map {
	unsigned start;
	unsigned len;
};
int cache_object(int c, int id, char *name, int sz, char *buf);
int cache_object_clean(int c, int id, char *name, int sz, char *buf);
int get_empty_cache(int (*)(struct ce_t *), int, int (*)(int, struct ce_t *, char *, int));
struct ce_t *find_cache_element(int c, unsigned id, char *name);
struct ce_t *add_cache_element(int cache, int id, char *name, int length, char *data);
int update_object(int c, int id, char *name, int off, int sz, char *buf, int true_size);
struct ce_t *remove_cached_element(int c, int id, char *name);
int free_cached_element(struct ce_t *c);
void sync_cache(int id, int red, int slow, int rm);
extern cache_t caches[NUM_CACHES];
int destroy_cache(int id, int );
int sync_element(int c, struct ce_t *e);
void remove_element(int c, struct ce_t *o);
void do_sync_of_mounted();
int kernel_cache_sync(int all, int);
int kernel_cache_sync_slow(int all);
void sync_dm();
#endif
