#include <sea/lib/linkedlist.h>
#include <sea/kobj.h>
struct linkedlist *linkedlist_create(struct linkedlist *list, int flags)
{
	KOBJ_CREATE(list, flags, LINKEDLIST_ALLOC);
	spinlock_create(&list->lock);
	list->head = &list->sentry;
	return list;
}

void linkedlist_destroy(struct linkedlist *list)
{
	KOBJ_DESTROY(list, LINKEDLIST_ALLOC);
}

void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj)
{
	spinlock_acquire(&list->lock);
	entry->next = list->head->next;
	entry->prev = list->head;
	entry->prev->next = entry;
	entry->next->prev = entry;
	entry->obj = obj;
	list->count++;
	spinlock_release(&list->lock);
}

static inline void __do_remove(struct linkedlist *list, struct linkedentry *entry)
{
	list->count--;
	entry->obj = NULL;
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

void linkedlist_remove(struct linkedlist *list, struct linkedentry *entry)
{
	spinlock_acquire(&list->lock);
	__do_remove(list, entry);
	spinlock_release(&list->lock);
}

void linkedlist_apply(struct linkedlist *list, bool (*fn)(struct linkedentry *))
{
	assert(fn);
	spinlock_acquire(&list->lock);
	struct linkedentry *ent = list->head->next;
	while(ent != &list->sentry) {
		struct linkedentry *next = ent->next;
		if(fn(ent)) {
			__do_remove(list, ent);
		}
		ent = next;
	}
	spinlock_release(&list->lock);
}

