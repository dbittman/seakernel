#ifndef __SEA_TM_TIMING_H
#define __SEA_TM_TIMING_H

#include <sea/types.h>

#define ONE_SECOND 1000 /* TODO: set to correct value */
#define ONE_MILLISECOND 1

void tm_thread_delay_sleep(time_t nanoseconds);
int tm_thread_delay(time_t nanoseconds);

#endif

