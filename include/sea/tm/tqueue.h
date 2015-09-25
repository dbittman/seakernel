#ifndef _TQUEUE_H
#define _TQUEUE_H

#include <sea/spinlock.h>
#include <sea/ll.h>
#define TQ_ALLOC 1

/* TODO: Redesign task queue system */

#define TQ_MAGIC 0xCAFED00D

struct tqueue {
	unsigned magic;
	unsigned flags;
	struct spinlock lock;
	unsigned num;
	struct llistnode *current;
	struct llist tql;
};

struct tqueue *tqueue_create(struct tqueue *tq, unsigned flags);
void tqueue_destroy(struct tqueue *tq);
struct llistnode *tqueue_insert(struct tqueue *tq, void *item, struct llistnode *node);
void tqueue_remove_entry(struct tqueue *tq, void *item);
void tqueue_remove_nolock(struct tqueue *tq, struct llistnode *i);
void tqueue_remove(struct tqueue *tq, struct llistnode *i);
void *tqueue_next(struct tqueue *tq);
extern struct tqueue *primary_queue;
extern struct llist *kill_queue;
#endif
