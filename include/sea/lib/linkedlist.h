#ifndef __SEA_LIB_LINKEDLIST_H
#define __SEA_LIB_LINKEDLIST_H

#define LINKEDLIST_ALLOC 1
#define LINKEDLIST_LOCKLESS 1
#define LINKEDLIST_MUTEX 2
struct linkedentry {
	void *obj;
	struct linkedentry *next, *prev;
};

#define linkedentry_obj(entry) ((entry) ? (entry)->obj : NULL)

#include <sea/spinlock.h>
#include <sea/asm/system.h>
#include <stdbool.h>

/* TODO: we should change mutex_t to struct mutex anyway */
struct __mutex_s;
struct linkedlist {
	struct linkedentry *head;
	struct linkedentry sentry;
	struct spinlock lock;
	_Atomic size_t count;
	int flags;
	struct __mutex_s *m_lock;
};

#define linkedlist_iter_end(list) &(list)->sentry
#define linkedlist_iter_start(list) (list)->head->next
#define linkedlist_iter_next(entry) (entry)->next

void __linkedlist_lock(struct linkedlist *list);
void __linkedlist_unlock(struct linkedlist *list);
void *linkedlist_head(struct linkedlist *list);
struct linkedlist *linkedlist_create(struct linkedlist *list, int flags);
void linkedlist_destroy(struct linkedlist *list);
void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj);
void linkedlist_remove(struct linkedlist *list, struct linkedentry *entry);
void linkedlist_do_remove(struct linkedlist *list, struct linkedentry *entry);
void linkedlist_apply(struct linkedlist *list, void (*fn)(struct linkedentry *));
void linkedlist_apply_data(struct linkedlist *list, void (*fn)(struct linkedentry *, void *data), void *);
struct linkedentry *linkedlist_find(struct linkedlist *list, bool (*fn)(struct linkedentry *, void *data), void *data);
unsigned long linkedlist_reduce(struct linkedlist *list, unsigned long (*fn)(struct linkedentry *, unsigned long), unsigned long init);
void linkedlist_apply_head(struct linkedlist *list, void (*fn)(struct linkedentry *));

#endif

