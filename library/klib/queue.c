#include <sea/lib/queue.h>
#include <sea/mutex.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>

struct queue *queue_create(struct queue *q, int flags)
{
	if(!q) {
		q = kmalloc(sizeof(struct queue));
		q->flags = QUEUE_ALLOC | flags;
	} else {
		q->flags = flags;
	}
	q->head = q->tail = 0;
	mutex_create(&q->lock, 0);
	return q;
}

void *queue_dequeue(struct queue *q)
{
	void *ret = 0;
	mutex_acquire(&q->lock);
	if(q->head) {
		struct queue_item *i = q->head;
		ret = q->head->ent;
		q->head = q->head->next;
		kfree(i);
		if(!q->head)
			q->tail = 0;
	}
	q->count--;
	mutex_release(&q->lock);
	return ret;
}

void queue_enqueue(struct queue *q, void *ent)
{
	mutex_acquire(&q->lock);
	struct queue_item *i = kmalloc(sizeof(struct queue_item));
	i->ent = ent;
	i->next = 0;
	if(q->tail) {
		q->tail->next = i;
		q->tail = i;
	} else {
		assert(!q->head);
		q->head = q->tail = i;
	}
	q->count++;
	mutex_release(&q->lock);
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

