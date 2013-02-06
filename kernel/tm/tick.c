#include <kernel.h>
#include <isr.h>
#include <task.h>
extern void ack(int);
extern unsigned read_epi();
extern void check_alarms();
extern unsigned heaping, waving;
extern int current_hz;
extern volatile long ticks;

int get_timer_th(int *t)
{
	if(t)
		*t = ticks;
	return current_hz;
}

/* Recurse through the parents of tasks and update their times */
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
	if(!current_task)
		return;
	if(current_task) {
		unsigned *t;
		++(*(current_task->system ? (t=(unsigned *)&current_task->stime) 
			: (t=(unsigned *)&current_task->utime)));
		/* This is a pretty damn awesome statement. Basically means 
		 * that we increment the parents t_c[u,s]time */
		lock_task_queue_reading(0);
		inc_parent_times(current_task->parent, 
			current_task->system ? __SYS : __USR);
		unlock_task_queue_reading(0);
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

void delay(int t)
{
	if(shutting_down)
		return (void) delay_sleep(t);
	long end = ticks + t + 1;
	if(!current_task || current_task->pid == 0)
	{
		__super_sti();
		while(ticks < end)
			schedule();
		return;
	}
	current_task->tick=end;
	current_task->state=TASK_ISLEEP;
	schedule();
}

void delay_sleep(int t)
{
	long end = ticks+t+1;
	__super_sti();
#if CONFIG_SMP
	lapic_eoi();
#endif
	while(ticks < end)
	{
#if CONFIG_SMP
		lapic_eoi();
#endif
		sti();
	}
}
