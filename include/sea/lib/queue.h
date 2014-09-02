#ifndef __SEA_LIB_QUEUE_H
#define __SEA_LIB_QUEUE_H

#include <sea/mutex.h>

struct queue_item {
	void *ent;
	struct queue_item *next;
};

struct queue {
	int flags, count;
	mutex_t lock;
	struct queue_item *head, *tail;
};

#define QUEUE_ALLOC 1

#endif

