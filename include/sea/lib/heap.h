#ifndef __SEA_LIB_HEAP_H
#define __SEA_LIB_HEAP_H

#include <sea/types.h>
#include <sea/rwlock.h>

#define HEAP_KMALLOC 1
#define HEAPMODE_MAX 0
#define HEAPMODE_MIN 1

struct heapnode {
	void *data;
	uint64_t key;
};

struct heap {
	int flags;
	int mode;
	size_t capacity;
	size_t count;
	rwlock_t rwl;
	struct heapnode *array;
};
#endif

