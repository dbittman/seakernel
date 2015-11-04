#include <sea/mm/kmalloc.h>
#include <sea/mm/reclaim.h>
#include <sea/lib/linkedlist.h>
#include <sea/lib/linkedlist.h>

struct linkedlist reclaimers;

void mm_reclaim_init(void)
{
	linkedlist_create(&reclaimers, LINKEDLIST_MUTEX);
}

void mm_reclaim_register(size_t (*fn)(void), size_t size)
{
	struct reclaimer *rec = kmalloc(sizeof(struct reclaimer));
	rec->size = size;
	rec->fn = fn;
	linkedlist_insert(&reclaimers, &rec->node, rec);
}

static unsigned long __reclaim_reducer(struct linkedentry *entry, unsigned long value)
{
	struct reclaimer *rec = entry->obj;
	return rec->fn() + value;
}

size_t mm_reclaim_size(size_t size)
{
	size_t amount = 0, thisround = 1;
	while(amount < size && thisround != 0) {
		thisround = linkedlist_reduce(&reclaimers, __reclaim_reducer, 0);
		amount += thisround;
	}
	return amount;
}

void mm_reclaim(void)
{
	linkedlist_reduce(&reclaimers, __reclaim_reducer, 0);
}

