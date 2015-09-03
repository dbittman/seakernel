#ifndef __SEA_LIB_TIMER_H
#define __SEA_LIB_TIMER_H

#include <sea/cpu/interrupt.h>

#define TIMER_MAGIC 0x123ABC00

#define TIMER_ALLOC	        0x1
#define TIMER_NO_CONCURRENT 0x2
#define TIMER_RUNNING       0x4

struct timer {
	int flags;
	unsigned int magic;
	unsigned long runs;
	unsigned long long start_time;
	unsigned long long max, min, last;
	double mean;
};

void timer_stop(struct timer *t);
int timer_start(struct timer *t);
void timer_destroy(struct timer *t);
struct timer *timer_create(struct timer *t, int flags);

static inline int __do_start_timer(struct timer *timer)
{
	int r = cpu_interrupt_set(0);
	timer_start(timer);
	return r;
}

static inline void __do_end_timer(struct timer *timer, int is)
{
	timer_stop(timer);
	cpu_interrupt_set(is);
}

#define BEGIN_TIMER(timer) int INTERRUPTSTATE##__FUNCTION__ = __do_start_timer(timer)
#define END_TIMER(timer) __do_end_timer(timer, INTERRUPTSTATE##__FUNCTION__)

#endif

