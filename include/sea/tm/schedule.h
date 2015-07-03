#ifndef _SEA_TM_SCHEDULE_H
#define _SEA_TM_SCHEDULE_H

#include <sea/cpu/registers.h>

int tm_schedule();
void tm_timer_handler(registers_t *);

int tm_get_current_frequency();
long tm_get_ticks();
void tm_set_current_frequency_indicator(int);

#endif
