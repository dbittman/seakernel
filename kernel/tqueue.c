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
	mutex_create(&tq->lock, MT_NOSCHED);
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

struct llistnode *tqueue_insert(tqueue_t *tq, void *item)
{
	int old = set_int(0);
	mutex_acquire(&tq->lock);
	struct llistnode *ret = ll_insert(&tq->tql, item);
	if(!tq->current)
		tq->current = tq->tql.head;
	mutex_release(&tq->lock);
	set_int(old);
	return ret;
}

void tqueue_remove_entry(tqueue_t *tq, void *item)
{
	int old = set_int(0);
	mutex_acquire(&tq->lock);
	if(tq->current->entry == item) tq->current=0;
	ll_remove_entry(&tq->tql, item);
	mutex_release(&tq->lock);
	set_int(old);
}

void tqueue_remove(tqueue_t *tq, struct llistnode *i)
{
	int old = set_int(0);
	mutex_acquire(&tq->lock);
	if(tq->current == i) tq->current=0;
	void *node = ll_do_remove(&tq->tql, i, 0);
	mutex_release(&tq->lock);
	set_int(old);
	kfree(node);
}

void *tqueue_next(tqueue_t *tq)
{
	int old = set_int(0);
	mutex_acquire(&tq->lock);
	if(tq->current) tq->current = tq->current->next;
	/* can't use else here. Need to catch the case when current.next is
	 * null above */
	if(!tq->current) tq->current = tq->tql.head;
	void *ret = tq->current->entry;
	mutex_release(&tq->lock);
	set_int(old);
	return ret;
}
