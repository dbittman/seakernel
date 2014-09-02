#ifndef _SEA_TM_SCHEDULE_H
#define _SEA_TM_SCHEDULE_H

#include <sea/cpu/registers.h>

#if CONFIG_SCHED_TTY
static int sched_tty = CONFIG_SCHED_TTY_AMOUNT;
#else
static int sched_tty = 0;
#endif

int tm_schedule();
void __tm_check_alarms();
void tm_timer_handler(registers_t *);

int tm_get_current_frequency();
long tm_get_ticks();
void tm_set_current_frequency_indicator(int);

#define TICKS_SECONDS(nsec) (tm_get_current_frequency() * nsec)

extern volatile unsigned int __allow_idle;

#endif
