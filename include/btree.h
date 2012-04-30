#ifndef _BTREE_H_
#define _BTREE_H_

// Platform dependent headers
#include <kernel.h>
#include <string.h>

#define mem_alloc kmalloc
#define mem_free kfree
#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0)
#define print kprintf

#define DEBUG_BTREE 0

#define bool char
#define false 0
#define true !false

typedef struct {
	void * root;
	unsigned min, max;
} bptree;







unsigned bptree_get_max_key(bptree *tree);
void bptree_delete(bptree *tree, unsigned key);
void bptree_destroy(bptree *tree);
bptree* bptree_create();
void *bptree_search(bptree *tree, unsigned key);
unsigned bptree_get_min_key(bptree *tree);
void bptree_insert(bptree *tree, unsigned key, void *ptr);





#endif
