#ifndef _SEA_TM__TM_H
#define _SEA_TM__TM_H

#include <sea/subsystem.h>

#if SUBSYSTEM != _SUBSYSTEM_TM
#error "_tm.h included from a non-tm source file"
#endif

#include <sea/tm/process.h>

/* process.c */
void __tm_engage_idle();
void __tm_disengage_idle();
int __tm_task_is_runable(task_t *task);

/* exit.c */
void __tm_move_task_to_kill_queue(task_t *t, int locked);

#endif
