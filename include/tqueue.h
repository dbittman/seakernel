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

tqueue_t *tqueue_create(tqueue_t *tq, unsigned flags);
void tqueue_destroy(tqueue_t *tq);
struct llistnode *tqueue_insert(tqueue_t *tq, void *item, struct llistnode *node);
void tqueue_remove_entry(tqueue_t *tq, void *item);
void tqueue_remove(tqueue_t *tq, struct llistnode *i);
void *tqueue_next(tqueue_t *tq);

#endif
