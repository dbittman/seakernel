#ifndef __SEA_LIB_LINKEDLIST_H
#define __SEA_LIB_LINKEDLIST_H

#include <sea/spinlock.h>
#include <stdbool.h>
#define LINKEDLIST_ALLOC 1

struct linkedentry {
	void *obj;
	struct linkedentry *next, *prev;
};

struct linkedlist {
	struct linkedentry *head;
	struct linkedentry sentry;
	struct spinlock lock;
	size_t count;
	int flags;
};

struct linkedlist *linkedlist_create(struct linkedlist *list, int flags);
void linkedlist_destroy(struct linkedlist *list);
void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj);
void linkedlist_remove(struct linkedlist *list, struct linkedentry *entry);
void linkedlist_apply(struct linkedlist *list, bool (*fn)(struct linkedentry *));

#endif

