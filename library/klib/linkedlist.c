#include <sea/lib/linkedlist.h>
#include <sea/kobj.h>
#include <stdbool.h>
#include <sea/mutex.h>
#include <sea/kernel.h>

inline void __linkedlist_lock(struct linkedlist *list)
{
	if(likely(!(list->flags & LINKEDLIST_LOCKLESS))) {
		if(unlikely(list->flags & LINKEDLIST_MUTEX))
			mutex_acquire(list->m_lock);
		else
			spinlock_acquire(&list->lock);
	}
}

inline void __linkedlist_unlock(struct linkedlist *list)
{
	if(likely(!(list->flags & LINKEDLIST_LOCKLESS))) {
		if(unlikely(list->flags & LINKEDLIST_MUTEX))
			mutex_release(list->m_lock);
		else
			spinlock_release(&list->lock);
	}
}

void *linkedlist_head(struct linkedlist *list)
{
	void *ret = NULL;
	__linkedlist_lock(list);
	if(list->head->next != &list->sentry)
		ret = list->head->next->obj;
	__linkedlist_unlock(list);
	return ret;
}

struct linkedlist *linkedlist_create(struct linkedlist *list, int flags)
{
	KOBJ_CREATE(list, flags, LINKEDLIST_ALLOC);
	if(!(flags & LINKEDLIST_LOCKLESS)) {
		if(flags & LINKEDLIST_MUTEX)
			list->m_lock = mutex_create(0, 0);
		else
			spinlock_create(&list->lock);
	}
	list->head = &list->sentry;
	list->head->next = list->head;
	list->head->prev = list->head;
	list->count = 0;
	return list;
}

void linkedlist_destroy(struct linkedlist *list)
{
	assert(list->head == &list->sentry);
	if(!(list->flags & LINKEDLIST_LOCKLESS)) {
		if(list->flags & LINKEDLIST_MUTEX)
			mutex_destroy(list->m_lock);
		else
			spinlock_destroy(&list->lock);
	}
	KOBJ_DESTROY(list, LINKEDLIST_ALLOC);
}

void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj)
{
	assert(list->head == &list->sentry);
	assert(list->head->next && list->head->prev);
	__linkedlist_lock(list);
	entry->next = list->head->next;
	entry->prev = list->head;
	entry->prev->next = entry;
	entry->next->prev = entry;
	entry->obj = obj;
	list->count++;
	assert(list->count > 0);
	__linkedlist_unlock(list);
}

void linkedlist_do_remove(struct linkedlist *list, struct linkedentry *entry)
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
	__linkedlist_lock(list);
	linkedlist_do_remove(list, entry);
	__linkedlist_unlock(list);
}

void linkedlist_apply(struct linkedlist *list, void (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
	assert(fn);
	__linkedlist_lock(list);
	struct linkedentry *ent = list->head->next;
	while(ent != &list->sentry) {
		/* fn could called linkedlist_remove, so we have to be ready for
		 * that possibility. */
		struct linkedentry *next = ent->next;
		fn(ent);
		ent = next;
	}
	__linkedlist_unlock(list);
}

void linkedlist_apply_data(struct linkedlist *list, void (*fn)(struct linkedentry *, void *), void *data)
{
	assert(list->head == &list->sentry);
	assert(fn);
	__linkedlist_lock(list);
	struct linkedentry *ent = list->head->next;
	while(ent != &list->sentry) {
		/* fn could called linkedlist_remove, so we have to be ready for
		 * that possibility. */
		struct linkedentry *next = ent->next;
		fn(ent, data);
		ent = next;
	}
	__linkedlist_unlock(list);
}


struct linkedentry *linkedlist_find(struct linkedlist *list, bool (*fn)(struct linkedentry *, void *data), void *data)
{
	assert(list->head == &list->sentry);
	assert(fn);
	__linkedlist_lock(list);
	struct linkedentry *ent = list->head->next;
	while(ent != &list->sentry) {
		if(fn(ent, data))
			break;
		ent = ent->next;
	}
	__linkedlist_unlock(list);
	if(ent == &list->sentry)
		return NULL;
	return ent;
}

unsigned long linkedlist_reduce(struct linkedlist *list, unsigned long (*fn)(struct linkedentry *, unsigned long), unsigned long init)
{
	assert(list->head == &list->sentry);
	assert(fn);
	__linkedlist_lock(list);
	struct linkedentry *ent = list->head->next;
	unsigned long current = init;
	while(ent != &list->sentry) {
		/* fn could called linkedlist_remove, so we have to be ready for
		 * that possibility. */
		struct linkedentry *next = ent->next;
		current = fn(ent, current);
		ent = next;
	}
	__linkedlist_unlock(list);
	return current;
}

void linkedlist_apply_head(struct linkedlist *list, void (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
	assert(fn);
	__linkedlist_lock(list);
	struct linkedentry *ent = list->head->prev;
	if(ent != &list->sentry) {
		assertmsg(list->count > 0,
				"count = %d, ent = %x (s=%x)\n", list->count, ent, &list->sentry);
		fn(ent);
	}
	__linkedlist_unlock(list);
}

