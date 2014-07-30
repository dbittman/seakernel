#include <sea/cpu/time.h>

unsigned long long time_get_epoch()
{
	return arch_time_get_epoch();
}

void time_get(struct tm *t)
{
	arch_time_get(t);
}

