#ifndef __SEA_LIB_HASH_SIMPLE_H
#define __SEA_LIB_HASH_SIMPLE_H

#include <sea/lib/hash.h>

static inline struct hash_table *hash_table_create_default(struct hash_table *h, int count)
{
	h = hash_table_create(h, 0, HASH_TYPE_CHAIN);
	hash_table_resize(h, HASH_RESIZE_MODE_IGNORE, count);
	hash_table_specify_function(h, HASH_FUNCTION_BYTE_SUM);
	return h;
}

#endif

