#include <sea/tm/ticker.h>
#include <sea/mm/kmalloc.h>
struct ticker *ticker_create(struct ticker *ticker, int flags)
{
	if(!ticker) {
		ticker = kmalloc(sizeof(struct ticker));
		ticker->flags = flags | TICKER_KMALLOC;
	} else {
		ticker->flags = flags;
	}
	ticker->tick = 0;
}

void ticker_tick(struct ticker *ticker, uint64_t nanoseconds)
{
	ticker->tick += nanoseconds;
}

void ticker_destroy(struct ticker *ticker)
{
	if(ticker->flags & TICKER_KMALLOC) {
		kfree(ticker);
	}
}

