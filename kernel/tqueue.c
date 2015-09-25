/* defines functions for task queues */
#include <sea/kernel.h>
#include <sea/tm/tqueue.h>
#include <sea/spinlock.h>
#include <sea/ll.h>
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
	ll_create_lockless(&tq->tql);
	tq->num=0;
	tq->magic = TQ_MAGIC;
	return tq;
}

void tqueue_destroy(struct tqueue *tq)
{
	spinlock_destroy(&tq->lock);
	ll_destroy(&tq->tql);
	if(tq->flags & TQ_ALLOC)
		kfree(tq);
}

struct llistnode *tqueue_insert(struct tqueue *tq, void *item, struct llistnode *node)
{
	spinlock_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	ll_do_insert(&tq->tql, node, item);
	if(!tq->current)
		tq->current = tq->tql.head;
	atomic_fetch_add_explicit(&tq->num, 1, memory_order_release);
	spinlock_release(&tq->lock);
	return node;
}

void tqueue_remove(struct tqueue *tq, struct llistnode *node)
{
	spinlock_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	if(tq->current == node) tq->current=0;
	ll_do_remove(&tq->tql, node, 0);
	atomic_fetch_sub_explicit(&tq->num, 1, memory_order_release);
	spinlock_release(&tq->lock);
}

/* tsearch may occasionally need to remove tasks from the queue
 * while the queue is locked, so we provide this for it */
void tqueue_remove_nolock(struct tqueue *tq, struct llistnode *i)
{
	assert(tq->magic == TQ_MAGIC);
	if(tq->current == i) tq->current=0;
	ll_do_remove(&tq->tql, i, 0);
	tq->num--;
}

/* this function may return null if there are no tasks in the queue */
void *tqueue_next(struct tqueue *tq)
{
	assert(tq->magic == TQ_MAGIC);
	spinlock_acquire(&tq->lock);
	assert(tq->num > 0);
	if(tq->current) tq->current = tq->current->next;
	/* can't use else here. Need to catch the case when current->next is
	 * null above */
	if(!tq->current) tq->current = tq->tql.head;
	assert(tq->current);
	void *ret = tq->current->entry;
	assert(ret);
	spinlock_release(&tq->lock);
	return ret;
}

