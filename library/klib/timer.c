#include <sea/cpu/atomic.h>
#include <sea/cpu/time.h>
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
	t->start_time = arch_hpt_get_nanoseconds();
	return 1;
}

void timer_stop(struct timer *t)
{
	unsigned long long end = arch_hpt_get_nanoseconds();
	assert(t->flags & TIMER_RUNNING);
	unsigned long long diff = end - t->start_time;
	t->last = diff;
	/* recalculate mean */
	size_t oldruns = t->runs++;
	t->mean = ((t->mean * oldruns) + diff) / (t->runs);
	if(t->max < diff)
		t->max = diff;
	if(t->min > diff)
		t->min = diff;
	and_atomic(&t->flags, ~TIMER_RUNNING);
}

