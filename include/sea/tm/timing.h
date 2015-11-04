#ifndef __SEA_TM_TIMING_H
#define __SEA_TM_TIMING_H

#include <sea/types.h>
#include <sea/cpu/registers.h>
#define ONE_SECOND 1000000
#define ONE_MILLISECOND 1000

void tm_thread_delay_sleep(time_t microseconds);
int tm_thread_delay(time_t microseconds);
void tm_timer_handler(struct registers *r, int, int);
int tm_get_current_frequency(void);
time_t tm_timing_get_microseconds(void);
void tm_set_current_frequency_indicator(int hz);
int tm_get_current_frequency(void);

#endif

