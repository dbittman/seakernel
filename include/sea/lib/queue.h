#ifndef __SEA_LIB_QUEUE_H
#define __SEA_LIB_QUEUE_H

#include <sea/mutex.h>

struct queue_item {
	void *ent;
	struct queue_item *next, *prev;
};

struct queue {
	int flags;
	_Atomic int count;
	mutex_t lock;
	struct queue_item *head, *tail;
};

#define QUEUE_ALLOC 1

struct queue *queue_create(struct queue *q, int flags);
void *queue_dequeue(struct queue *q);
void queue_enqueue(struct queue *q, void *ent);
void queue_enqueue_item(struct queue *q, struct queue_item *i, void *ent);
void *queue_remove(struct queue *q, struct queue_item *item);
void *queue_peek(struct queue *q);
void queue_destroy(struct queue *q);
struct queue_item *queue_dequeue_item(struct queue *q);

#define queue_count(q) q->count

#endif

