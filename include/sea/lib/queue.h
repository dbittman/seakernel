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

struct queue *queue_create(struct queue *q, int flags);
void *queue_dequeue(struct queue *q);
void queue_enqueue(struct queue *q, void *ent);
void *queue_peek(struct queue *q);
void queue_destroy(struct queue *q);

#define queue_count(q) q->count

#endif

