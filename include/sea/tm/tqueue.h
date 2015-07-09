#ifndef _TQUEUE_H
#define _TQUEUE_H

#include <sea/mutex.h>
#include <sea/ll.h>
#define TQ_ALLOC 1

/* TODO: Redesign task queue system */

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

struct tqueue {
	unsigned magic;
	unsigned flags;
	mutex_t lock;
	volatile unsigned num;
	volatile struct llistnode *current;
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
