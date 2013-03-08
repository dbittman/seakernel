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
	#warning "better place for this?"
	__engage_idle();
	cpu_t *cpu = prev->cpu;
	task_t *t = tqueue_next(cpu->active_queue);
	while(t)
	{
		assert(t);
		if(unlikely(t->magic != TASK_MAGIC))
			panic(0, "Invalid task (%d:%d): %x", t->pid, t->state, t->magic);
		/* this handles everything in the "active queue". This includes
		 * running tasks, tasks that have timed blocks... */
		update_task(t);
		if(task_is_runable(t))
			return t;
		t = tqueue_next(cpu->active_queue);
		/* This way the kernel can sleep without being in danger of 
		 * causing a lockup. Basically, if the kernel is the only
		 * runnable task, it gets forced to run */
		if(t && t == prev && !task_is_runable(t))
			return (task_t *)kernel_task;
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
	if(current_task->sigd && !(current_task->flags & TF_INSIG) 
		&& !(current_task->flags & TF_KTASK) && current_task->pid)
	{
		current_task->flags |= TF_INSIG;
		/* Jump to the signal handler */
		handle_signal((task_t *)current_task);
		/* if we've gotten here, then we are interruptible or running.
		 * set the state to running since interruptible tasks fully
		 * wake up when signaled */
		current_task->state = TASK_RUNNING;
	}
	if(current_task->flags & TF_SETINT) {
		current_task->flags &= ~TF_SETINT;
		set_int(1);
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
	set_kernel_stack(new->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	/* keep track of when we got to run */
	new->slice = ticks;
}
/*This is the magic super awesome and important kernel function 'schedule()'. 
 * It is arguable the most important function. Here we store the current 
 * task's context, search for the next process to run, and load it's context.*/
void schedule()
{
	if(unlikely(!current_task || !kernel_task))
		return;
	if(set_int(0))
		current_task->flags |= TF_SETINT;
	task_t *old = current_task;
	store_context();
	volatile task_t *new = (volatile task_t *)get_next_task(old);
	set_current_task_dp(new, 0 /* TODO: this should be the current CPU */);
	restore_context(new);
	asm("         \
		mov %1, %%esp;       \
		mov %2, %%ebp;       \
		mov %3, %%cr3;"
	: : "r"(0), "r"(new->esp), "r"(new->ebp), 
			"r"(new->pd[1023]&PAGE_MASK) : "eax");
	if(likely(!(new->flags & TF_FORK)))
		return (void) post_context_switch();
	new->flags &= ~TF_FORK;
	asm("jmp *%0"::"r"(current_task->eip));
}

void check_alarms()
{
	task_t *t = alarm_list_start;
	while(t) {
		if(unlikely(!(--t->alrm_count)))
		{
			task_t *r = alarm_list_start;
			while(r && r->alarm_next != t) r = r->alarm_next;
			if(!r) {
				assert(t == alarm_list_start);
				alarm_list_start = t->alarm_next;
			} else {
				assert(r->alarm_next == t);
				r->alarm_next = t->alarm_next;
				t->alarm_next=0;
			}
			t->flags &= ~TF_ALARM;
			t->sigd = SIGALRM;
		}
		t=t->alarm_next;
	}
}
