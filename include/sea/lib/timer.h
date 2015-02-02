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

#endif

