#ifndef _SEA_TM_SCHEDULE_H
#define _SEA_TM_SCHEDULE_H

#if SCHED_TTY
static int sched_tty = SCHED_TTY_CYC;
#else
static int sched_tty = 0;
#endif

int tm_schedule();


#endif
