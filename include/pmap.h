#ifndef __PMAP_H
#define __PMAP_H

#include <types.h>
#include <mutex.h>

struct pmap {
	uint32_t magic;
	unsigned flags;
	int idx_max, idx;
	addr_t *phys, *virt;
	mutex_t lock;
};

#define PMAP_INITIAL_MAX 64
#define PMAP_MAGIC 0xBEEF5A44

#define PMAP_ALLOC 1

void pmap_destroy(struct pmap *m);
struct pmap *pmap_create(struct pmap *m, unsigned flags);
addr_t pmap_get_mapping(struct pmap *m, addr_t p);

#endif
