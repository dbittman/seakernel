#include <sea/lib/queue.h>
#include <sea/mutex.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>
#include <stdatomic.h>

struct queue *queue_create(struct queue *q, int flags)
{
	if(!q) {
		q = kmalloc(sizeof(struct queue));
		q->flags = QUEUE_ALLOC | flags;
	} else {
		q->flags = flags;
	}
	q->head = q->tail = 0;
	q->count = ATOMIC_VAR_INIT(0);
	mutex_create(&q->lock, MT_NOSCHED);
	return q;
}

void *queue_remove(struct queue *q, struct queue_item *item)
{
	void *ret = 0;
	mutex_acquire(&q->lock);
	assert(q->count > 0 && q->head && q->tail);

	if(item->prev)
		item->prev->next = item->next;
	else
		q->head = item->next;
	if(item->next)
		item->next->prev = item->prev;
	else
		q->tail = item->prev;

	atomic_fetch_sub_explicit(&q->count, 1, memory_order_release);
	ret = item->ent;
	mutex_release(&q->lock);
	return ret;
}

struct queue_item *queue_dequeue_item(struct queue *q)
{
	mutex_acquire(&q->lock);
	struct queue_item *item = q->head;
	if(!item) {
		assert(!q->count);
		mutex_release(&q->lock);
		return 0;
	}
	assert(q->count);

	if(item->prev)
		item->prev->next = item->next;
	else
		q->head = item->next;
	if(item->next)
		item->next->prev = item->prev;
	else
		q->tail = item->prev;

	atomic_fetch_sub_explicit(&q->count, 1, memory_order_release);
	mutex_release(&q->lock);
	return item;
}

void *queue_dequeue(struct queue *q)
{
	struct queue_item *i = queue_dequeue_item(q);
	void *ret = 0;
	if(i) {
		ret = i->ent;
		kfree(i);
	}
	return ret;
}

void queue_enqueue_item(struct queue *q, struct queue_item *i, void *ent)
{
	mutex_acquire(&q->lock);
	i->ent = ent;
	i->next = i->prev = 0;
	if(q->tail) {
		q->tail->next = i;
		i->prev = q->tail;
		q->tail = i;
	} else {
		assert(!q->head);
		assert(!q->count);
		q->head = q->tail = i;
	}
	atomic_fetch_add_explicit(&q->count, 1, memory_order_release);
	mutex_release(&q->lock);
}

void queue_enqueue(struct queue *q, void *ent)
{
	struct queue_item *i = kmalloc(sizeof(struct queue_item));
	queue_enqueue_item(q, i, ent);
}

void *queue_peek(struct queue *q)
{
	void *ret = 0;
	mutex_acquire(&q->lock);
	if(q->head)
		ret = q->head->ent;
	mutex_release(&q->lock);
	return ret;
}

void queue_destroy(struct queue *q)
{
	mutex_destroy(&q->lock);
	if(q->flags & QUEUE_ALLOC)
		kfree(q);
}

