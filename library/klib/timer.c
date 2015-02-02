#include <sea/tm/schedule.h>
#include <sea/cpu/atomic.h>
#include <sea/lib/timer.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>

struct timer *timer_create(struct timer *t, int flags)
{
	if(!t) {
		t = kmalloc(sizeof(struct timer));
		t->flags = flags | TIMER_ALLOC;
	} else
		t->flags = flags;
	t->magic = TIMER_MAGIC;
	t->min = ~0;
	return t;
}

void timer_destroy(struct timer *t)
{
	assert(t && t->magic == TIMER_MAGIC);
	if(t->flags & TIMER_ALLOC)
		kfree(t);
}

int timer_start(struct timer *t)
{
	assert(t && t->magic == TIMER_MAGIC);
	if(ff_or_atomic(&t->flags, TIMER_RUNNING) & TIMER_RUNNING) {
		if((t->flags & TIMER_NO_CONCURRENT))
			panic(0, "tried to start timer when timer was running");
		return 0;
	}
	t->start_time = tm_get_ticks();
	return 1;
}

void timer_stop(struct timer *t)
{
	unsigned long long end = tm_get_ticks();
	unsigned long long diff = end - t->start_time;
	assert(t->flags & TIMER_RUNNING);
	/* recalculate mean */
	t->mean = ((t->mean * t->runs) + diff) / (++t->runs);
	if(t->max < diff)
		t->max = diff;
	if(t->min > diff)
		t->min = diff;
	and_atomic(&t->flags, ~TIMER_RUNNING);
}

