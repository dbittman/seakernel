#include <sea/mm/kmalloc.h>
#include <sea/lib/heap.h>
#include <sea/kernel.h>
#include <sea/errno.h>

#define CHILDA(n) (2*n+1)
#define CHILDB(n) (2*n+2)
#define PARENT(n) ((n-1) / 2)

struct heap *heap_create(struct heap *heap, int flags, int heapmode)
{
	if(!heap) {
		heap = kmalloc(sizeof(struct heap));
		heap->flags = flags | HEAP_KMALLOC;
	} else {
		heap->flags = flags;
	}
	heap->count = 0;
	heap->capacity = 64;
	heap->mode = heapmode;
	if(!(flags & HEAP_LOCKLESS))
		rwlock_create(&heap->rwl);
	heap->array = kmalloc(sizeof(struct heapnode) * heap->capacity);
	return heap;
}

static void __heap_resize(struct heap *heap)
{
	size_t newcap = heap->capacity * 2;
	struct heapnode *newarray = kmalloc(sizeof(struct heapnode) * newcap);
	memcpy(newarray, heap->array, sizeof(struct heapnode) * heap->count);
	struct heapnode *oldarray = heap->array;
	heap->array = newarray;
	heap->capacity = newcap;
	kfree(oldarray);
}

static void __swap(struct heap *heap, size_t e1, size_t e2)
{
	struct heapnode tmp = heap->array[e1];
	heap->array[e1] = heap->array[e2];
	heap->array[e2] = tmp;
}

static void __heap_bubbleup(struct heap *heap, size_t elem)
{
	if(!elem)
		return;
	uint64_t kp = heap->array[PARENT(elem)].key;
	uint64_t cur = heap->array[elem].key;

	if(heap->mode == HEAPMODE_MAX) {
		if(cur > kp) {
			__swap(heap, elem, PARENT(elem));
			__heap_bubbleup(heap, PARENT(elem));
		}
	} else {
		if(cur < kp) {
			__swap(heap, elem, PARENT(elem));
			__heap_bubbleup(heap, PARENT(elem));
		}
	}
}

static void __heap_bubbledown(struct heap *heap, size_t elem)
{
	uint64_t k1, k2, cur;
	k1 = heap->array[CHILDA(elem)].key;
	k2 = heap->array[CHILDB(elem)].key;
	cur = heap->array[elem].key;
	if(heap->mode == HEAPMODE_MAX) {
		if(cur < k1 && cur < k2) {
			__swap(heap, elem, k1 > k2 ? CHILDA(elem) : CHILDB(elem));
			__heap_bubbledown(heap, k1 > k2 ? CHILDA(elem) : CHILDB(elem));
		} else if(cur < k1) {
			__swap(heap, elem, CHILDA(elem));
			__heap_bubbledown(heap, CHILDA(elem));
		} else if(cur < k2) {
			__swap(heap, elem, CHILDB(elem));
			__heap_bubbledown(heap, CHILDB(elem));
		}
	} else {
		if(cur > k1 && cur > k2) {
			__swap(heap, elem, k1 < k2 ? CHILDA(elem) : CHILDB(elem));
			__heap_bubbledown(heap, k1 < k2 ? CHILDA(elem) : CHILDB(elem));
		} else if(cur > k1) {
			__swap(heap, elem, CHILDA(elem));
			__heap_bubbledown(heap, CHILDA(elem));
		} else if(cur > k2) {
			__swap(heap, elem, CHILDB(elem));
			__heap_bubbledown(heap, CHILDB(elem));
		}
	}
}

void heap_insert(struct heap *heap, uint64_t key, void *data)
{
	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_acquire(&heap->rwl, RWL_WRITER);

	if(heap->count == heap->capacity) {
		__heap_resize(heap);
	}

	heap->array[heap->count].key = key;
	heap->array[heap->count].data = data;

	__heap_bubbleup(heap, heap->count++);

	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_release(&heap->rwl, RWL_WRITER);
}

int heap_peek(struct heap *heap, uint64_t *key, void **data)
{
	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_acquire(&heap->rwl, RWL_READER);
	if(!heap->count) {
		if(!(heap->flags & HEAP_LOCKLESS))
			rwlock_release(&heap->rwl, RWL_READER);
		return -ENOENT;
	}

	if(key) {
		*key = heap->array[0].key;
	}
	if(data) {
		*data = heap->array[0].data;
	}

	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_release(&heap->rwl, RWL_READER);
	return 0;
}

int heap_pop(struct heap *heap, uint64_t *key, void **data)
{
	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_acquire(&heap->rwl, RWL_WRITER);
	if(!heap->count) {
		if(!(heap->flags & HEAP_LOCKLESS))
			rwlock_release(&heap->rwl, RWL_WRITER);
		return -ENOENT;
	}

	if(key) {
		*key = heap->array[0].key;
	}
	if(data) {
		*data = heap->array[0].data;
	}
	/* standard heap stuff */
	heap->array[0] = heap->array[--heap->count];
	__heap_bubbledown(heap, 0);

	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_release(&heap->rwl, RWL_WRITER);
	return 0;
}

void heap_destroy(struct heap *heap)
{
	assert(!heap->count);
	if(!(heap->flags & HEAP_LOCKLESS))
		rwlock_destroy(&heap->rwl);
	kfree(heap->array);
	if(heap->flags & HEAP_KMALLOC)
		kfree(heap);
}

