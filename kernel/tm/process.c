#include <sea/subsystem.h>
#define SUBSYSTEM _SUBSYSTEM_TM
#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/schedule.h>
void tm_process_enter_system(int sys)
{
	current_task->system=(!sys ? -1 : sys);
	current_task->cur_ts/=2;
}

void tm_process_exit_system()
{
	current_task->last = current_task->system;
	current_task->system=0;
}

void tm_process_pause(task_t *t)
{
	/* don't care what other processors do */
	t->state = TASK_ISLEEP;
	if(t == current_task) {
		while(!tm_schedule());
	}
}

void tm_process_resume(task_t *t)
{
	t->state = TASK_RUNNING;
}

void tm_engage_idle()
{
	tm_process_resume((task_t *)kernel_task);
}

void __tm_disengage_idle()
{
	tm_process_pause((task_t *)kernel_task);
}

int __tm_process_is_runable(task_t *task)
{
	assert(task);
	if(task->state == TASK_DEAD)
		return 0;
	return (int)(task->state == TASK_RUNNING 
	|| task->state == TASK_SUICIDAL 
	|| (task->state == TASK_ISLEEP && (task->sigd)));
}
