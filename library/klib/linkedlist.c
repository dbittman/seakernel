#include <sea/lib/linkedlist.h>
#include <sea/kobj.h>
#include <stdbool.h>
#include <sea/kernel.h>
struct linkedlist *linkedlist_create(struct linkedlist *list, int flags)
{
	KOBJ_CREATE(list, flags, LINKEDLIST_ALLOC);
	if(!(flags & LINKEDLIST_LOCKLESS))
		spinlock_create(&list->lock);
	list->head = &list->sentry;
	list->head->next = list->head;
	list->head->prev = list->head;
	list->count = 0;
	return list;
}

void linkedlist_destroy(struct linkedlist *list)
{
	assert(list->head == &list->sentry);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_destroy(&list->lock);
	KOBJ_DESTROY(list, LINKEDLIST_ALLOC);
}

void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj)
{
	assert(list->head == &list->sentry);
	assert(list->head->next && list->head->prev);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_acquire(&list->lock);
	entry->next = list->head->next;
	entry->prev = list->head;
	entry->prev->next = entry;
	entry->next->prev = entry;
	entry->obj = obj;
	list->count++;
	assert(list->count > 0);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_release(&list->lock);
}

static inline void __do_remove(struct linkedlist *list, struct linkedentry *entry)
{
	assert(entry != &list->sentry);
	assert(list->count > 0);
	list->count--;
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

void linkedlist_remove(struct linkedlist *list, struct linkedentry *entry)
{
	assert(list->head == &list->sentry);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_acquire(&list->lock);
	__do_remove(list, entry);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_release(&list->lock);
}

void linkedlist_apply(struct linkedlist *list, void (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
	assert(fn);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_acquire(&list->lock);
	struct linkedentry *ent = list->head->next;
	while(ent != &list->sentry) {
		/* fn could called linkedlist_remove, so we have to be ready for
		 * that possibility. */
		struct linkedentry *next = ent->next;
		fn(ent);
		ent = next;
	}
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_release(&list->lock);
}

void linkedlist_apply_head(struct linkedlist *list, void (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
	assert(fn);
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_acquire(&list->lock);
	struct linkedentry *ent = list->head->prev;
	if(ent != &list->sentry) {
		assertmsg(list->count > 0,
				"count = %d, ent = %x (s=%x)\n", list->count, ent, &list->sentry);
		fn(ent);
	}
	if(!(list->flags & LINKEDLIST_LOCKLESS))
		spinlock_release(&list->lock);
}

