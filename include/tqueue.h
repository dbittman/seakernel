#ifndef _TQUEUE_H
#define _TQUEUE_H

#include <mutex.h>
#include <ll.h>
#define TQ_ALLOC 1

typedef struct {
	unsigned flags;
	mutex_t lock;
	struct llistnode *current;
	struct llist tql;
} tqueue_t;

#endif
