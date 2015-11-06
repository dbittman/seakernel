#ifndef __SEA_CPU_TIME_H
#define __SEA_CPU_TIME_H

#include <sea/types.h>
typedef long clock_t;

struct tm
{
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};
typedef long suseconds_t;
struct timeval {
	time_t tv_sec;
	suseconds_t tv_usec;
};

struct tms {
	clock_t tms_utime;
	clock_t tms_stime;
	clock_t tms_cutime;
	clock_t tms_cstime;
};

time_t arch_time_get_epoch();
time_t time_get_epoch();
void arch_time_get(struct tm *now);
uint64_t arch_hpt_get_nanoseconds();
void time_get(struct tm *);
int sys_gettimeofday(struct timeval *tv, void *g);

#endif

