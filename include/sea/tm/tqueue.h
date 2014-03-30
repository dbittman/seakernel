#ifndef _TQUEUE_H
#define _TQUEUE_H

#include <sea/mutex.h>
#include <sea/ll.h>
#include <task.h>
#define TQ_ALLOC 1

#define TSEARCH_FINDALL           0x1
#define TSEARCH_PID               0x2
#define TSEARCH_UID               0x4
#define TSEARCH_EUID              0x8
#define TSEARCH_TTY              0x10
#define TSEARCH_PARENT           0x20
#define TSEARCH_ENUM             0x40
#define TSEARCH_EXIT_WAITING     0x80
#define TSEARCH_EXIT_PARENT     0x100
#define TSEARCH_EXCLUSIVE       0x200
#define TSEARCH_ENUM_ALIVE_ONLY 0x400

#define TQ_MAGIC 0xCAFED00D

typedef struct {
	unsigned magic;
	unsigned flags;
	mutex_t lock;
	volatile unsigned num;
	volatile struct llistnode *current;
	struct llist tql;
} tqueue_t;

tqueue_t *tqueue_create(tqueue_t *tq, unsigned flags);
void tqueue_destroy(tqueue_t *tq);
struct llistnode *tqueue_insert(tqueue_t *tq, void *item, struct llistnode *node);
void tqueue_remove_entry(tqueue_t *tq, void *item);
void tqueue_remove_nolock(tqueue_t *tq, struct llistnode *i);
void tqueue_remove(tqueue_t *tq, struct llistnode *i);
void *tqueue_next(tqueue_t *tq);
task_t *tm_search_tqueue(tqueue_t *tq, unsigned flags, unsigned long value, void (*action)(task_t *, int), int arg, int *);
extern tqueue_t *primary_queue;

#endif
