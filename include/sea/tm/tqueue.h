#ifndef _TQUEUE_H
#define _TQUEUE_H

#include <sea/spinlock.h>
#include <sea/lib/linkedlist.h>
#define TQ_ALLOC 1

/* TODO: Redesign task queue system */

#define TQ_MAGIC 0xCAFED00D

struct tqueue {
	unsigned magic;
	unsigned flags;
	struct spinlock lock;
	unsigned num;
	struct linkedentry *current;
	struct linkedlist tql;
};

struct tqueue *tqueue_create(struct tqueue *tq, unsigned flags);
void tqueue_destroy(struct tqueue *tq);
struct linkedentry *tqueue_insert(struct tqueue *tq, void *item, struct linkedentry *node);
void tqueue_remove(struct tqueue *tq, struct linkedentry *i);
void *tqueue_next(struct tqueue *tq);
#endif
