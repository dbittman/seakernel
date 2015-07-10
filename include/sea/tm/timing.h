#ifndef __SEA_TM_TIMING_H
#define __SEA_TM_TIMING_H

#include <sea/types.h>
#include <sea/cpu/registers.h>
#define ONE_SECOND 1000000
#define ONE_MILLISECOND 1000

void tm_thread_delay_sleep(time_t microseconds);
int tm_thread_delay(time_t microseconds);
void tm_timer_handler(registers_t *r);
int tm_get_current_frequency(void);

#endif

