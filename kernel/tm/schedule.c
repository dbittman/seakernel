/* Functions for scheduling tasks */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>
#include <context.h>

__attribute__((always_inline)) inline void update_task(task_t *t)
{
	/* task's delay ran out */
	if((t->state == TASK_USLEEP || t->state == TASK_ISLEEP) && t->tick <= ticks && t->tick)
		t->state = TASK_RUNNING;
}
/* This here is the basic scheduler - It does nothing 
 * except find the next runable task */
__attribute__((always_inline)) inline task_t *get_next_task(task_t *prev)
{
	assert(prev && kernel_task);
	cpu_t *cpu = prev->cpu;
	assert(cpu);
	task_t *t = tqueue_next(cpu->active_queue);
	while(t)
	{
		if(unlikely(t->magic != TASK_MAGIC))
			panic(0, "Invalid task (%d:%d): %x", t->pid, t->state, t->magic);
		/* this handles everything in the "active queue". This includes
		 * running tasks, tasks that have timed blocks... */
		update_task(t);
		if(task_is_runable(t) && !(t->flags & TF_MOVECPU))
			return t;
		t = tqueue_next(cpu->active_queue);
		/* This way the kernel can sleep without being in danger of 
		 * causing a lockup. Basically, if the kernel is the only
		 * runnable task, it gets forced to run */
		if(t && t == prev && !task_is_runable(t)) {
			/* make sure to update the state in case it slept */
			cpu->ktask->state = TASK_RUNNING;
			return (task_t *)cpu->ktask;
		}
	}
	panic(PANIC_NOSYNC, "get_next_task(): Task became null pointer!", t);
	return (task_t *)0;
}

__attribute__((always_inline)) static inline void post_context_switch()
{
	if(unlikely(current_task->state == TASK_SUICIDAL))
		task_suicide();
	/* We only process signals if we aren't in a system call.
	 * this is because if a task is suddenly interrupted inside an
	 * important syscall while doing something important the results
	 * could be very bad. Any syscall that waits will need to include
	 * a method of detecting signals and returning safely. */
	if(current_task->sigd 
		&& (!(current_task->flags & TF_INSIG) 
		   || signal_will_be_fatal(current_task, current_task->sigd))
		&& !(current_task->flags & TF_KTASK) && current_task->pid
		&& !(current_task->flags & TF_EXITING))
	{
		current_task->flags |= TF_INSIG;
		/* Jump to the signal handler */
		handle_signal((task_t *)current_task);
		/* if we've gotten here, then we are interruptible or running.
		 * set the state to running since interruptible tasks fully
		 * wake up when signaled */
		current_task->state = TASK_RUNNING;
	}
	assert(!get_cpu_interrupt_flag());
	if(current_task->flags & TF_SETINT) {
		/* should never enable interrupts inside an interrupt, except for
		 * syscalls */
		assert(!(current_task->flags & TF_IN_INT) || current_task->sysregs);
		current_task->flags &= ~TF_SETINT;
		assert(!set_int(1));
	}
}

int schedule()
{
	if(unlikely(!current_task || !kernel_task))
		return 0;
	if((((cpu_t *)current_task->cpu)->flags & CPU_LOCK))
		return 0;
	if(!(((cpu_t *)current_task->cpu)->flags & CPU_TASK))
		return 0;
	assert(!(current_task->flags & TF_SETINT));
	/* make sure to re-enable interrupts when we come back to this
	 * task if we entered schedule with them enabled */
	if(set_int(0))
		current_task->flags |= TF_SETINT;
	task_t *old = current_task;
	cpu_t *cpu = (cpu_t *)old->cpu;
	assert(cpu && cpu->cur == old);
	mutex_acquire(&cpu->lock);
	store_context();
	volatile task_t *next_task = (volatile task_t *)get_next_task(old);
	assert(cpu == next_task->cpu);
	restore_context(next_task);
	next_task->slice = ticks;
	((cpu_t *)next_task->cpu)->cur = next_task;
	/* we need to call this after restore_context because in restore_context
	 * we access new->cpu */
	mutex_release(&cpu->lock);
	context_switch(next_task);
	reset_timer_state();
	/* tasks that have come from fork() (aka, new tasks) have this
	 * flag set, such that here we just to their entry point in fork() */
	if(likely(!(next_task->flags & TF_FORK)))
	{
		post_context_switch();
		return 1;
	}
	next_task->flags &= ~TF_FORK;
	asm("jmp *%0"::"r"(current_task->eip));
	/* we never get here, but lets keep gcc happy */
	return 1;
}

void check_alarms()
{
	if(!alarm_list_start) return;
	/* interrupts will be disabled here. Thus, we can aquire 
	 * a mutex safely */
	mutex_acquire(alarm_mutex);
	if((unsigned)ticks > alarm_list_start->alarm_end)
	{
		alarm_list_start->flags &= ~TF_ALARM;
		alarm_list_start->sigd = SIGALRM;
		alarm_list_start = alarm_list_start->alarm_next;
		alarm_list_start->alarm_prev = 0;
	}
	mutex_release(alarm_mutex);
}
