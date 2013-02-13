/* defines functions for task queues */
#include <kernel.h>
#include <tqueue.h>
#include <mutex.h>
#include <ll.h>

tqueue_t *tqueue_create(tqueue_t *tq, unsigned flags)
{
	if(!tq) {
		tq = (void *)kmalloc(sizeof(tqueue_t));
		tq->flags = (TQ_ALLOC | flags);
	} else
		tq->flags=flags;
	mutex_create(&tq->lock, 0);
	ll_create_lockless(&tq->tql);
	return tq;
}

void tqueue_destroy(tqueue_t *tq)
{
	mutex_destroy(&tq->lock);
	ll_destroy(&tq->tql);
	if(tq->flags & TQ_ALLOC)
		kfree(tq);
}

void tqueue_insert(tqueue_t *tq, void *item)
{
	mutex_acquire(&tq->lock);
	ll_insert(&tq->tql, item);
	if(!tq->current)
		tq->current = tq->tql.head;
	mutex_release(&tq->lock);
}

void tqueue_remove_entry(tqueue_t *tq, void *item)
{
	mutex_acquire(&tq->lock);
	if(tq->current->entry == item) tq->current=0;
	ll_remove_entry(&tq->tql, item);
	mutex_release(&tq->lock);
}

void tqueue_remove(tqueue_t *tq, struct llistnode *i)
{
	mutex_acquire(&tq->lock);
	if(tq->current == i) tq->current=0;
	ll_remove(&tq->tql, i);
	mutex_release(&tq->lock);
}

void *tqueue_next(tqueue_t *tq)
{
	mutex_acquire(&tq->lock);
	if(tq->current) tq->current = tq->current->next;
	/* can't use else here. Need to catch the case when current.next is
	 * null above */
	if(!tq->current) tq->current = tq->tql.head;
	void *ret = tq->current->entry;
	mutex_release(&tq->lock);
	return ret;
}
