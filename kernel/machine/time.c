#include <sea/cpu/time.h>
#include <sea/types.h>
#include <sea/errno.h>
#include <sea/tm/timing.h>
time_t time_get_epoch(void)
{
	return arch_time_get_epoch();
}

void time_get(struct tm *t)
{
	arch_time_get(t);
}

int sys_gettimeofday(struct timeval *tv, void *g)
{
	if(!tv)
		return -EINVAL;
	tv->tv_sec = arch_time_get_epoch();
	tv->tv_usec = tm_timing_get_microseconds() % (1000 * 1000);
}

