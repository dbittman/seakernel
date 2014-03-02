#include <sea/subsystem.h>
#define SUBSYSTEM _SUBSYSTEM_TM
#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>
int current_hz=1000;
volatile long ticks=0;
int sys_get_timer_th(int *t)
{
	if(t)
		*t = ticks;
	return current_hz;
}

static int GET_MAX_TS(task_t *t)
{
	if(t->flags & TF_EXITING)
		return 1;
	int x = t->priority;
	if(t->tty == curcons->tty)
		x += sched_tty;
	return x;
}

/* Iterate through the parents of tasks and update their times */
static void inc_parent_times(task_t *t, int u, int s)
{
	while(t && t != kernel_task) {
		t->t_cutime += u;
		t->t_cstime += s;
		t=t->parent;
	}
}

static void do_run_scheduler()
{
	if(!current_task ||
		(current_task->flags&TF_DYING) || 
		(current_task->flags&TF_LOCK))
		return;
	tm_schedule();
}

#define __SYS 0, 1
#define __USR 1, 0
static void do_tick()
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
	__tm_check_alarms();
	if(current_task != kernel_task) {
		if(__tm_process_is_runable(current_task) && current_task->cur_ts>0 
				&& --current_task->cur_ts)
			return;
		else if(current_task->cur_ts <= 0)
			current_task->cur_ts = GET_MAX_TS(current_task);
	}
	do_run_scheduler();
}

void tm_timer_handler(registers_t *r)
{
	/* prevent multiple cpus from adding to ticks */
	/* TODO */
	//if(!current_task || !current_task->cpu || ((cpu_t *)current_task->cpu) == primary_cpu)
		add_atomic(&ticks, 1);
	/* engage the idle task occasionally */
	if((ticks % current_hz*10) == 0)
		tm_engage_idle();
	do_tick();
}

void tm_delay(int t)
{
	if((kernel_state_flags & KSF_SHUTDOWN))
		return (void) tm_delay_sleep(t);
	long end = ticks + t + 1;
	if(!current_task || current_task->pid == 0)
	{
		interrupt_set(1);
		while(ticks < end)
			tm_schedule();
		return;
	}
	current_task->tick=end;
	current_task->state=TASK_ISLEEP;
	while(!tm_schedule());
}

void tm_delay_sleep(int t)
{
	long end = ticks+t+1;
	int old = interrupt_set(1);
	int to=100000;
	long start = ticks;
	while(ticks < end) {
		asm("pause");
		interrupt_set(1);
		if(!--to && start == ticks) {
			printk(4, "[tm]: tm_delay_sleep reached timeout!\n");
			break;
		}
	}
	interrupt_set(old);
}
