/* defines functions for task queues */
#include <sea/kernel.h>
#include <sea/tm/tqueue.h>
#include <sea/spinlock.h>
#include <sea/lib/linkedlist.h>
#include <sea/cpu/processor.h>
#include <stdatomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/mm/kmalloc.h>
#include <sea/spinlock.h>

struct tqueue *tqueue_create(struct tqueue *tq, unsigned flags)
{
	if(!tq) {
		tq = (void *)kmalloc(sizeof(struct tqueue));
		tq->flags = (TQ_ALLOC | flags);
	} else
		tq->flags=flags;
	spinlock_create(&tq->lock);
	linkedlist_create(&tq->tql, LINKEDLIST_LOCKLESS);
	tq->num=0;
	tq->magic = TQ_MAGIC;
	return tq;
}

void tqueue_destroy(struct tqueue *tq)
{
	spinlock_destroy(&tq->lock);
	linkedlist_destroy(&tq->tql);
	if(tq->flags & TQ_ALLOC)
		kfree(tq);
}

struct linkedentry *tqueue_insert(struct tqueue *tq, void *item, struct linkedentry *node)
{
	spinlock_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	linkedlist_insert(&tq->tql, node, item);
	if(!tq->current)
		tq->current = linkedlist_iter_start(&tq->tql);
	atomic_fetch_add_explicit(&tq->num, 1, memory_order_release);
	spinlock_release(&tq->lock);
	return node;
}

void tqueue_remove(struct tqueue *tq, struct linkedentry *node)
{
	spinlock_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	if(tq->current == node) tq->current=0;
	linkedlist_remove(&tq->tql, node);
	atomic_fetch_sub_explicit(&tq->num, 1, memory_order_release);
	spinlock_release(&tq->lock);
}

/* this function may return null if there are no tasks in the queue */
void *tqueue_next(struct tqueue *tq)
{
	assert(tq->magic == TQ_MAGIC);
	spinlock_acquire(&tq->lock);
	assert(tq->num > 0);
	if(tq->current)
		tq->current = linkedlist_iter_next(tq->current);
	/* can't use else here. Need to catch the case when current->next is
	 * null above */
	if(!tq->current || tq->current == linkedlist_iter_end(&tq->tql))
		tq->current = linkedlist_iter_start(&tq->tql);
	assert(tq->current);
	void *ret = linkedentry_obj(tq->current);
	assert(ret);
	spinlock_release(&tq->lock);
	return ret;
}

