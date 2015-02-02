#ifndef __SEA_LIB_TIMER_H
#define __SEA_LIB_TIMER_H

#define TIMER_MAGIC 0x123ABC00

#define TIMER_ALLOC	        0x1
#define TIMER_NO_CONCURRENT 0x2
#define TIMER_RUNNING       0x4

struct timer {
	int flags;
	unsigned int magic;
	unsigned long runs;
	unsigned long long start_time;
	unsigned long long max, min;
	double mean;
};

void timer_stop(struct timer *t);
int timer_start(struct timer *t);
void timer_destroy(struct timer *t);
struct timer *timer_create(struct timer *t, int flags);

#endif

