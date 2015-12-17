#include <sea/mutex.h>
#include <sea/lib/charbuffer.h>
#include <sea/kobj.h>
#include <sea/tm/thread.h>

struct charbuffer *charbuffer_create(struct charbuffer *cb, int flags, size_t cap)
{
	KOBJ_CREATE(cb, flags, CHARBUFFER_ALLOC);
	if(!(flags & CHARBUFFER_LOCKLESS))
		mutex_create(&cb->lock, 0);
	cb->buffer = kmalloc(sizeof(unsigned char) * cap);
	blocklist_create(&cb->readers, 0, "charbuffer-readers");
	blocklist_create(&cb->writers, 0, "charbuffer-writers");
	cb->cap = cap;
	return cb;
}

void charbuffer_destroy(struct charbuffer *cb)
{
	KOBJ_DESTROY(cb, CHARBUFFER_ALLOC);
}

static bool __release_lock(void *data)
{
	struct charbuffer *cb = data;
	if(!(cb->flags & CHARBUFFER_LOCKLESS))
	mutex_release(&cb->lock);
	return true;
}

size_t charbuffer_read(struct charbuffer *cb, unsigned char *out, size_t length, bool block)
{
	size_t i = 0;
	if(!(cb->flags & CHARBUFFER_LOCKLESS))
		mutex_acquire(&cb->lock);

	while(i < length) {
		if(cb->count > 0) {
			*out++ = cb->buffer[cb->tail++ % cb->cap];
			cb->count--;
			i++;
		} else if(i == 0 && block) {
			if(atomic_exchange(&cb->eof, 0)) {
				if(!(cb->flags & CHARBUFFER_LOCKLESS))
					mutex_release(&cb->lock);
				return 0;
			}
			/* no data - block */
			tm_blocklist_wakeall(&cb->writers);
			int r = tm_thread_block_confirm(&cb->readers,
					THREADSTATE_INTERRUPTIBLE, __release_lock, cb);
			if(r != 0)
				return i;
			if(!(cb->flags & CHARBUFFER_LOCKLESS))
				mutex_acquire(&cb->lock);
		} else {
			break;
		}
	}
	tm_blocklist_wakeall(&cb->writers);
	if(!(cb->flags & CHARBUFFER_LOCKLESS))
		mutex_release(&cb->lock);
	return i;
}

size_t charbuffer_write(struct charbuffer *cb, unsigned char *in, size_t length, bool block)
{
	size_t i = 0;
	if(!(cb->flags & CHARBUFFER_LOCKLESS))
		mutex_acquire(&cb->lock);

	while(i < length) {
		if(cb->count < cb->cap) {
			cb->buffer[cb->head++ % cb->cap] = *in++;
			cb->count++;
			i++;
		} else if(!(cb->flags & CHARBUFFER_DROP) && block) {
			/* full - block */
			tm_blocklist_wakeall(&cb->readers);
			int r = tm_thread_block_confirm(&cb->writers, THREADSTATE_INTERRUPTIBLE,
					__release_lock, cb);
			if(r != 0)
				return i;
			if(!(cb->flags & CHARBUFFER_LOCKLESS))
				mutex_acquire(&cb->lock);
		} else {
			break;
		}
	}
	tm_blocklist_wakeall(&cb->readers);
	if(!(cb->flags & CHARBUFFER_LOCKLESS))
		mutex_release(&cb->lock);
	return i;
}

