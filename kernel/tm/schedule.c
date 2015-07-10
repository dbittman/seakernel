#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/schedule.h>
#include <sea/asm/system.h>
#include <sea/syscall.h>

static int GET_MAX_TS(struct thread *t)
{
	if(t->flags & TF_EXITING)
		return 1;
	int x = t->priority;
	/* process gets a boost if it's on the current console */
	if(t->tty == current_console->tty)
		x += sched_tty;
	return x;
}

static __attribute__((always_inline)) inline void update_task(struct thread *t)
{
	/* task's tm_delay ran out */
	if((t->state == TASK_USLEEP || t->state == TASK_ISLEEP) && t->tick <= tm_get_ticks() && t->tick)
		t->state = TASK_RUNNING;
}
/* This here is the basic scheduler - It does nothing 
 * except find the next runable task */
static __attribute__((always_inline)) inline task_t *get_next_task(task_t *prev, struct cpu *cpu)
{
	assert(prev && kernel_task);
	assert(prev->cpu == cpu);
	assert(cpu);
	
	if(prev != kernel_task) {
		if(__tm_process_is_runable(prev) && prev->cur_ts>0 
				&& --prev->cur_ts)
		{
			/* current task hasn't used up its time slice yet */
			return prev;
		}
		else if(prev->cur_ts <= 0)
			prev->cur_ts = GET_MAX_TS(prev);
	}
	
	task_t *t = tqueue_next(cpu->active_queue);
	while(t)
	{
		assert(t);
		if(unlikely(t->magic != TASK_MAGIC))
			panic(0, "Invalid task (%d:%d): %x", t->pid, t->state, t->magic);
		/* this handles everything in the "active queue". This includes
		 * running tasks, tasks that have timed blocks... */
		update_task(t);
		if(__tm_process_is_runable(t))
			return t;
		t = tqueue_next(cpu->active_queue);
		/* This way the kernel can sleep without being in danger of 
		 * causing a lockup. If the kernel task is the only
		 * runnable task, it gets forced to run */
		if(t && t == prev && !__tm_process_is_runable(t)) {
			/* make sure to update the state in case it slept */
			assert(cpu->ktask);
			cpu->ktask->state = TASK_RUNNING;
			return (task_t *)cpu->ktask;
		}
	}
	panic(PANIC_NOSYNC, "get_next_task(): Task became null pointer!", t);
	return (task_t *)0;
}

__attribute__((always_inline)) static inline void check_signals(void)
{
	if(unlikely(current_task->state == TASK_SUICIDAL) && !(current_task->flags & TF_EXITING))
		tm_process_suicide();
	/* We only process signals if we aren't in a system call.
	 * this is because if a task is suddenly interrupted inside an
	 * important syscall while doing something important the results
	 * could be very bad. Any syscall that waits will need to include
	 * a method of detecting signals and returning safely. */
	if(current_task->sigd 
		&& (!(current_task->flags & TF_INSIG) 
		   || tm_signal_will_be_fatal(current_task, current_task->sigd))
		&& !(current_task->flags & TF_KTASK) && current_task->pid
		&& !(current_task->flags & TF_EXITING) && !(current_task->system == SYS_FORK))
	{
		tm_raise_flag(TF_INSIG);
		/* Jump to the signal handler */
		int new_state = __tm_handle_signal((task_t *)current_task);
		/* if we've gotten here, then we are interruptible or running.
		 * set the state since interruptible tasks fully
		 * wake up when signaled. */
		current_task->state = new_state;
	}
}

__attribute__((always_inline)) static inline void post_context_switch(void)
{
	assert(!(kernel_state_flags & KSF_SHUTDOWN) || current_task->flags & TF_SHUTDOWN);
	assert(!cpu_interrupt_get_flag());
	int enable_interrupts = 0;
	if(current_task->flags & TF_SETINT) {
		/* should never enable interrupts inside an interrupt, except for
		 * syscalls */
		assert(!(current_task->flags & TF_IN_INT) || current_task->sysregs);
		tm_lower_flag(TF_SETINT);
		enable_interrupts = 1;
	}
	
	check_signals();
	
	if(enable_interrupts)
		cpu_interrupt_set(1);
}

int tm_schedule(void)
{
	if(unlikely(!current_task || !kernel_task))
		return 0;
	if((current_task->cpu->flags & CPU_LOCK))
		return 0;
	if(!(current_task->cpu->flags & CPU_TASK))
		return 0;
	assert(!(kernel_state_flags & KSF_SHUTDOWN) || current_task->flags & TF_SHUTDOWN);
	if(kernel_state_flags & KSF_SHUTDOWN) return 1;
	assert(current_task->magic == TASK_MAGIC);
	if(current_task->thread) assert(current_task->thread->magic == THREAD_MAGIC);
	
	/* make sure to re-enable interrupts when we come back to this
	 * task if we entered schedule with them enabled */
	task_t *old = current_task;
	struct cpu *cpu = old->cpu;
	task_t *next_task = (task_t *)get_next_task(old, cpu);
	
	if(old == next_task) {
		/* if we've chose the current task to run again, no need for a 
		 * full context switch. But we need to check signals. */
		check_signals();
		return 0;
	}

	/* we're going to mess with interrupt state here. Cause, ya know, context
	 * switching. Store the current state so that when we switch back to this
	 * process, we can restore it later */
	if(cpu_interrupt_set(0)) {
		assert(!(current_task->flags & TF_SETINT));
		tm_raise_flag(TF_SETINT);
	} else
		assert(!(current_task->flags & TF_SETINT));
	
	assert(cpu && cpu->cur == old);
	
	mutex_acquire(&cpu->lock);
	store_context();
	/* the tm_exiting task has fully 'exited' and has now scheduled out of
	 * itself. It will never be scheduled again, and the page directory
	 * will never be accessed again */
	if(old->flags & TF_DYING) {
		assert(old->state == TASK_DEAD);
		tm_raise_flag(TF_BURIED);
	}
	old->syscall_count = 0;
	
	assert(next_task);
	assert(cpu == next_task->cpu);
	restore_context(next_task);
	next_task->slice = tm_get_ticks();
	next_task->cpu->cur = next_task;
	
	/* we need to call this after restore_context because in restore_context
	 * we access new->cpu */
	mutex_release(&cpu->lock);
	/* after calling context switch, we may NOT use any variables that
	 * were used above it, since they are not necessarily valid. */
	context_switch(next_task);
	assert(current_task->magic == TASK_MAGIC);
	if(current_task->thread) assert(current_task->thread->magic == THREAD_MAGIC);
	//reset_timer_state(); /* TODO: This may be needed... */
	/* tasks that have come from fork() (aka, new tasks) have this
	 * flag set, such that here we just jump to their entry point in fork() */
	if(likely(!(current_task->flags & TF_FORK)))
	{
		post_context_switch();
		return 1;
	}
	tm_process_lower_flag(current_task, TF_FORK);
	cpu_interrupt_set(1);
	arch_cpu_jump(current_task->eip);
	/* we never get here, but lets keep gcc happy */
	return 1;
}

void __tm_check_alarms(void)
{
	if(!alarm_list_start) return;
	/* interrupts will be disabled here. Thus, we can aquire 
	 * a mutex safely */
	mutex_acquire(alarm_mutex);
	if((unsigned)tm_get_ticks() > alarm_list_start->alarm_end)
	{
		tm_process_lower_flag(alarm_list_start, TF_ALARM);
		alarm_list_start->sigd = SIGALRM;
		alarm_list_start = alarm_list_start->alarm_next;
		alarm_list_start->alarm_prev = 0;
	}
	mutex_release(alarm_mutex);
}

