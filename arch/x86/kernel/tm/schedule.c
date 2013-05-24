/* Functions for scheduling tasks */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>

void _overflow(char *type)
{
	printk(5, "%s overflow occurred in task %d (esp=%x, ebp=%x, heap_end=%x). Killing...\n", 
			type, current_task->pid, current_task->esp, current_task->ebp, 
			current_task->heap_end);
#if DEBUG
	panic(0, "Overflow");
#endif
	task_suicide();
}

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
		if(t && t == prev && !task_is_runable(t))
			return (task_t *)cpu->ktask;
	}
	///return cpu->ktask;
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
		&& current_task->system != 26)
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

__attribute__((always_inline)) static inline void store_context()
{
	asm("mov %%esp, %0" : "=r"(current_task->esp));
	asm("mov %%ebp, %0" : "=r"(current_task->ebp));
	/* Check for stack and heap overflow */
	if(!current_task->esp || (!(current_task->esp >= TOP_TASK_MEM_EXEC && current_task->esp < TOP_TASK_MEM) 
			&& !(current_task->esp >= KMALLOC_ADDR_START && current_task->esp < KMALLOC_ADDR_END)))
		_overflow("stack");
	if(current_task->heap_end && current_task->heap_end >= TOP_USER_HEAP)
		_overflow("heap");
	if(current_task->flags & TF_DYING)
		current_task->flags |= TF_BURIED;
}

__attribute__((always_inline)) static inline void restore_context(task_t *new)
{
	/* Update some last-minute things. The stack. */
	set_kernel_stack(current_tss, new->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	/* keep track of when we got to run */
	new->slice = ticks;
	((cpu_t *)new->cpu)->cur = new;
}

void schedule()
{
	if(unlikely(!current_task || !kernel_task))
		return;
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
	volatile task_t *new = (volatile task_t *)get_next_task(old);
	assert(cpu == new->cpu);
	restore_context(new);
	/* we need to call this after restore_context because in restore_context
	 * we access new->cpu */
	mutex_release(&cpu->lock);
	asm("         \
		mov %1, %%esp;       \
		mov %2, %%ebp;       \
		mov %3, %%cr3;"
	: : "r"(0), "r"(new->esp), "r"(new->ebp), 
			"r"(new->pd[1023]&PAGE_MASK) : "eax");
	/* tasks that have come from fork() (aka, new tasks) have this
	 * flag set, such that here we just to their entry point in fork() */
	if(likely(!(new->flags & TF_FORK)))
		return (void) post_context_switch();
	new->flags &= ~TF_FORK;
	asm("jmp *%0"::"r"(current_task->eip));
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
