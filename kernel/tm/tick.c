#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>
int current_hz=1000;
volatile long ticks=0;
int get_timer_th(int *t)
{
	if(t)
		*t = ticks;
	return current_hz;
}

/* Iterate through the parents of tasks and update their times */
void inc_parent_times(task_t *t, int u, int s)
{
	while(t && t != kernel_task) {
		t->t_cutime += u;
		t->t_cstime += s;
		t=t->parent;
	}
}

inline static void do_run_scheduler()
{
	if(!current_task ||
		(current_task->flags&TF_DYING) || 
		(current_task->flags&TF_LOCK))
		return;
	schedule();
}

void run_scheduler()
{
	do_run_scheduler();
}

#define __SYS 0, 1
#define __USR 1, 0
void do_tick()
{
	if(!current_task || (kernel_state_flags&KSF_PANICING))
		return;
	if(!(((cpu_t *)current_task->cpu)->flags & CPU_TASK))
		return;
	if(current_task) {
		current_task->system 
			? (++current_task->stime) 
			: (++current_task->utime);
		inc_parent_times(current_task->parent, 
			current_task->system ? __SYS : __USR);
	}
	check_alarms();
	if(current_task != kernel_task) {
		if(task_is_runable(current_task) && current_task->cur_ts>0 
				&& --current_task->cur_ts)
			return;
		else if(current_task->cur_ts <= 0)
			current_task->cur_ts = GET_MAX_TS(current_task);
	}
	do_run_scheduler();
}

void timer_handler(registers_t r)
{
	/* prevent multiple cpus from adding to ticks */
	if(!current_task || !current_task->cpu || ((cpu_t *)current_task->cpu) == primary_cpu)
		add_atomic(&ticks, 1);
	/* engage the idle task occasionally */
	if((ticks % current_hz*10) == 0)
		__engage_idle();
	do_tick();
}

void delay(int t)
{
	if((kernel_state_flags & KSF_SHUTDOWN))
		return (void) delay_sleep(t);
	long end = ticks + t + 1;
	if(!current_task || current_task->pid == 0)
	{
		set_int(1);
		while(ticks < end)
			schedule();
		return;
	}
	current_task->tick=end;
	current_task->state=TASK_ISLEEP;
	while(!schedule());
}

void delay_sleep(int t)
{
	long end = ticks+t+1;
	int old = set_int(1);
	while(ticks < end) {
		asm("pause");
		set_int(1);
	}
	set_int(old);
}
