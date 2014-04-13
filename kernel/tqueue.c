/* defines functions for task queues */
#include <sea/kernel.h>
#include <sea/tm/tqueue.h>
#include <sea/mutex.h>
#include <sea/ll.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
/* the rules for tqueue's are simple:
 * 1) you MUST disable interrupts when accessing them. This makes
 *    it safe for single-cpu machines to use these.
 * 2) you MUST acquire the mutex to access them. This makes it safe
 *    for multi-cpu machines to use them too.
 * 3) While accessing them, you MAY NOT call any functions that may
 *    possibly reschedule, enable interrupts, or access tqueues.
 */

tqueue_t *tqueue_create(tqueue_t *tq, unsigned flags)
{
	if(!tq) {
		tq = (void *)kmalloc(sizeof(tqueue_t));
		tq->flags = (TQ_ALLOC | flags);
	} else
		tq->flags=flags;
	mutex_create(&tq->lock, MT_NOSCHED);
	ll_create_lockless(&tq->tql);
	tq->num=0;
	tq->magic = TQ_MAGIC;
	return tq;
}

void tqueue_destroy(tqueue_t *tq)
{
	mutex_destroy(&tq->lock);
	ll_destroy(&tq->tql);
	if(tq->flags & TQ_ALLOC)
		kfree(tq);
}

struct llistnode *tqueue_insert(tqueue_t *tq, void *item, struct llistnode *node)
{
	int old = cpu_interrupt_set(0);
	mutex_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	ll_do_insert(&tq->tql, node, item);
	if(!tq->current)
		tq->current = tq->tql.head;
	add_atomic(&tq->num, 1);
	mutex_release(&tq->lock);
	assert(!cpu_interrupt_set(old));
	return node;
}

void tqueue_remove(tqueue_t *tq, struct llistnode *node)
{
	int old = cpu_interrupt_set(0);
	mutex_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	if(tq->current == node) tq->current=0;
	ll_do_remove(&tq->tql, node, 0);
	sub_atomic(&tq->num, 1);
	mutex_release(&tq->lock);
	assert(!cpu_interrupt_set(old));
}

/* tsearch may occasionally need to remove tasks from the queue
 * while the queue is locked, so we provide this for it */
void tqueue_remove_nolock(tqueue_t *tq, struct llistnode *i)
{
	assert(tq->magic == TQ_MAGIC);
	if(tq->current == i) tq->current=0;
	ll_do_remove(&tq->tql, i, 0);
	tq->num--;
}

/* this function may return null if there are no tasks in the queue */
void *tqueue_next(tqueue_t *tq)
{
	int old = cpu_interrupt_set(0);
	mutex_acquire(&tq->lock);
	assert(tq->magic == TQ_MAGIC);
	assert(tq->num > 0);
	if(tq->current) tq->current = tq->current->next;
	/* can't use else here. Need to catch the case when current->next is
	 * null above */
	if(!tq->current) tq->current = tq->tql.head;
	assert(tq->current);
	void *ret = tq->current->entry;
	assert(ret);
	mutex_release(&tq->lock);
	assert(!cpu_interrupt_set(old));
	return ret;
}
