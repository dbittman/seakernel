#include <stdatomic.h>
#include <sea/cpu/time.h>
#include <sea/lib/timer.h>
#include <sea/mm/kmalloc.h>
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <stdbool.h>
#include <sea/vsprintf.h>
static _Atomic uint32_t mean_difference = 0;
static bool timers_calibrated = false;
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
	unsigned long long test = arch_hpt_get_nanoseconds();
	if(!test)
		return 0;
	assert(t && t->magic == TIMER_MAGIC);
	if(atomic_fetch_or(&t->flags, TIMER_RUNNING) & TIMER_RUNNING) {
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
	if(atomic_fetch_and(&t->flags, ~TIMER_RUNNING) & TIMER_RUNNING) {
		unsigned long long diff = (end - t->start_time);
		if(diff > mean_difference)
			diff -= mean_difference;
		else
			diff = 1;
		t->last = diff;
		if(timers_calibrated) {
			/* recalculate mean */
			size_t oldruns = t->runs++;
			t->mean = ((t->mean * oldruns) + diff) / (t->runs);
			t->recent_mean = ((t->recent_mean * 1000) + diff) / (1001);
			if(t->max < diff)
				t->max = diff;
			if(t->min > diff)
				t->min = diff;
		}
	}
}

void timer_calibrate(void)
{
	struct timer t;
	uint32_t mean = 0;
	timer_create(&t, 0);
	printk(0, "[timer]: calibrating HPT timers...\n");
	int old = cpu_interrupt_set(0);
	for(int i=0;i<10000;i++) {
		int r = timer_start(&t);
		atomic_thread_fence(memory_order_seq_cst);
		timer_stop(&t);
		atomic_thread_fence(memory_order_seq_cst);
		mean = ((mean * i) + t.last) / (i+1);
		cpu_pause();
		assert(r);
	}
	mean_difference = mean;
	atomic_thread_fence(memory_order_seq_cst);
	timers_calibrated = true;
	printk(0, "[timer]: found mean base value of %d\n", mean_difference);
	cpu_interrupt_set(old);
	timer_destroy(&t);
}

