#include <sea/lib/linkedlist.h>
#include <sea/kobj.h>
#include <stdbool.h>
#include <sea/kernel.h>
struct linkedlist *linkedlist_create(struct linkedlist *list, int flags)
{
	KOBJ_CREATE(list, flags, LINKEDLIST_ALLOC);
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
	KOBJ_DESTROY(list, LINKEDLIST_ALLOC);
}

void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj)
{
	assert(list->head == &list->sentry);
	assert(list->head->next && list->head->prev);
	spinlock_acquire(&list->lock);
	entry->next = list->head->next;
	entry->prev = list->head;
	entry->prev->next = entry;
	entry->next->prev = entry;
	entry->obj = obj;
	list->count++;
	assert(list->count > 0);
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
	spinlock_acquire(&list->lock);
	__do_remove(list, entry);
	spinlock_release(&list->lock);
}

/* NOTE: TODO: This would call fn with preempt disabled. Is that okay? Is it okay
 * to disable preempt while we're looping though every element? */
void linkedlist_apply(struct linkedlist *list, bool (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
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

/* NOTE: TODO: This would call fn with preempt disabled. Is that okay? Is it okay
 * to disable preempt while we're looping though every element? */
void linkedlist_apply_head(struct linkedlist *list, bool (*fn)(struct linkedentry *))
{
	assert(list->head == &list->sentry);
	assert(fn);
	spinlock_acquire(&list->lock);
	struct linkedentry *ent = list->head->prev;
	if(ent != &list->sentry) {
		if(fn(ent)) {
			__do_remove(list, ent);
		}
	}
	spinlock_release(&list->lock);
}

