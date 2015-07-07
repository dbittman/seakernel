#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/schedule.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
static int current_hz=1000;
static volatile long ticks=0;

int tm_get_current_frequency()
{
	return current_hz;
}

void tm_set_current_frequency_indicator(int hz)
{
	current_hz = hz;
}

long tm_get_ticks()
{
	return ticks;
}

int sys_get_timer_th(int *t)
{
	if(t)
		*t = ticks;
	return current_hz;
}

/* Iterate through the parents of tasks and update their times */
static void inc_parent_times(struct process *t, int u, int s)
{
	/* TODO: better way to do this? */
	while(t) {
		//t->t_cutime += u;
		//t->t_cstime += s;
		t=t->parent;
	}
}

static void do_run_scheduler()
{
	if(!current_thread)
		return;
	tm_schedule();
}

#define __SYS 0, 1
#define __USR 1, 0
static void do_tick()
{
	if(current_thread) {
		ticker_tick(current_thread->cpu->ticker, 1000 /* TODO: Whatever this actually is */);
		//current_thread->system 
		//	? (++current_process->stime) 
		//	: (++current_process->utime);
		//inc_parent_times(current_process->parent, 
		//	current_thread->system ? __SYS : __USR);
	}
	/* TODO: alarm() */
	/* TODO: maybe set flag to schedule */
}

void tm_timer_handler(registers_t *r)
{
	/* prevent multiple cpus from adding to ticks */
	/* TODO */
	//if(!current_task || !current_task->cpu || current_task->cpu == primary_cpu)
		add_atomic(&ticks, 1);
	do_tick();
}
/* TODO */
#if 0
void tm_delay(int t)
{
	if((kernel_state_flags & KSF_SHUTDOWN))
		return (void) tm_delay_sleep(t);
	long end = ticks + t + 1;
	if(!current_task || current_task->pid == 0)
	{
		cpu_interrupt_set(1);
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
	int old = cpu_interrupt_set(1);
	int to=100000;
	long start = ticks;
	while(ticks < end) {
		cpu_pause();
		cpu_interrupt_set(1);
		if(!--to && start == ticks) {
			printk(4, "[tm]: tm_delay_sleep reached timeout!\n");
			break;
		}
	}
	cpu_interrupt_set(old);
}
#endif

