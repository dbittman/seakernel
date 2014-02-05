#include <kernel.h>
#include <lib/hash.h>
#include <memory.h>

struct hash_collision_resolver *hash_collision_resolvers[NUM_HASH_COLLISION_RESOLVERS] = {
	0,
	0,
	0,
	0,
	0
};

struct hash_table *hash_table_create(struct hash_table *h, unsigned flags, unsigned type)
{
	if(!h) {
		h = kmalloc(sizeof(struct hash_table));
		h->flags |= (HASH_ALLOC | flags);
	} else
		h->flags = flags;
	
	assert(type < NUM_HASH_COLLISION_RESOLVERS);
	h->resolver = hash_collision_resolvers[type];
	if(!h->resolver)
		panic(0, "hash table collision resolver %d requested but not implemented!", type);
	h->size = 0;
	h->entries = 0;
	return h;
}

int hash_table_resize(struct hash_table *h, unsigned mode)
{
	if(!h || !h->magic) panic(0, "hash resize on invalid hash table");
	int old_size = h->size;
	
	
	
	
	
	
	
	
	return old_size;
}

