/* Functions for scheduling tasks */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#if DEBUG
#include <elf.h>
#endif

mutex_t scheding;

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
	if(t->state == TASK_USLEEP)
	{
		if(t->tick <= ticks && t->tick)
			t->state = TASK_RUNNING;
	}
	else if(t->state == TASK_ISLEEP)
	{
		if(t->waitflag) {
			if(!t->waiting_true){
				if((*(t->waitflag)) == t->wait_for)
					t->state = TASK_RUNNING;
			} else {
				if((*(t->waitflag)) != t->wait_for)
					t->state = TASK_RUNNING;
			}
		}
	}
}
/* This here is the basic scheduler - It does nothing 
 * except find the next runable task */
__attribute__((always_inline)) inline task_t *get_next_task()
{
	assert(current_task && kernel_task);
	task_t *prev = (task_t *)current_task;
	task_t *t = (task_t *)(prev->next);
	if(!t) t = (task_t *)kernel_task;
	while(t)
	{
		assert(t);
		if(t->magic != TASK_MAGIC)
			panic(0, "Invalid task (%d:%d): %x", t->pid, t->state, t->magic);
		update_task(t);
		if(task_is_runable(t))
			return t;
		if(!(t = t->next))
			t = (task_t *)kernel_task;
		/* This way the kernel can sleep without being in danger of 
		 * causing a lockup. Basically, if the kernel is the only
		 * runnable task, it gets forced to run */
		if(t == prev && !task_is_runable(t))
			return (task_t *)kernel_task;
	}
	panic(PANIC_NOSYNC, "get_next_task(): Task became null pointer!");
	return (task_t *)0;
}

__attribute__((always_inline)) static inline void post_context_switch()
{
	if(current_task->state == TASK_SUICIDAL) {
		task_critical();
		task_suicide();
		panic(PANIC_NOSYNC, "Suicide failed");
	}
	if(current_task->flags & TF_REQMEM)
	{
		/* We have been asked to calculate out memory usage. */
		task_critical();
		current_task->flags &= ~TF_REQMEM;
		current_task->mem_usage_calc = get_mem_usage();
		current_task->state = current_task->old_state;
		task_uncritical();
		force_schedule();
		return;
	}
	/* We only process signals if we aren't in a system call.
	 * this is because if a task is suddenly interrupted inside an
	 * important syscall while doing something important the results
	 * could be very bad. Any syscall that waits will need to include
	 * a method of detecting signals and returning safely. */
	if(current_task->sigd && !current_task->system && !(current_task->flags & TF_INSIG))
	{
		current_task->flags |= TF_INSIG;
		current_task->state = TASK_RUNNING;
		/* Jump to the signal handler */
		handle_signal((task_t *)current_task);
	}
#if DEBUG
	if(current_task->pid != 0 && !current_task->system && current_task->regs && current_task->regs->useresp > 0xC0000000) {
		const char *name = elf_lookup_symbol(current_task->regs->eip, &kernel_elf);
		panic(0, "A: %d: %x: %s", current_task->system, current_task->regs->eip, name);
	}
#endif
}

__attribute__((always_inline)) static inline void store_context(unsigned eip)
{
	u32int esp, ebp;
	asm("mov %%esp, %0" : "=r"(esp));
	asm("mov %%ebp, %0" : "=r"(ebp));
	/* No, we didn't switch tasks. Save some register values and switch */
	current_task->eip = eip;
	current_task->esp = esp;
	current_task->ebp = ebp;
	/* Check for stack over-run */
	if(!esp || (!(esp >= TOP_TASK_MEM_EXEC && esp < TOP_TASK_MEM) 
			&& !(esp >= KMALLOC_ADDR_START && esp < KMALLOC_ADDR_END)))
		_overflow("stack");
	if(current_task->heap_end && current_task->heap_end >= TOP_USER_HEAP)
		_overflow("heap");
}

__attribute__((always_inline)) static inline void restore_context()
{
	/* Update some last-minute things. The stack. */
	set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
}

/*This is the magic super awesome and important kernel function 'schedule()'. 
 * It is arguable the most important function. Here we store the current 
 * task's context, search for the next process to run, and load it's context.*/
unsigned globa;
void schedule()
{
	__super_cli();
	if(unlikely(!current_task || !kernel_task))
		return;
	u32int esp, ebp, eip;
	eip = read_eip();
	
	//if (eip == 0xFFFFFFFF) {
	//	post_context_switch();
	//	return;
	//}
	globa = eip;
	if(unlikely(!eip))
		panic(PANIC_NOSYNC, "schedule(): Invalid eip");
	
	store_context(eip);
	volatile task_t *new = (volatile task_t *)get_next_task();
	set_current_task_dp(new, 0 /* TODO: this should be the current CPU */);
	restore_context();
	asm("         \
		mov %1, %%esp;       \
		mov %2, %%ebp;       \
		mov %3, %%cr3;       \
		mov $0xFFFFFFFF, %%eax;" \
	: : "r"(current_task->eip), "r"(current_task->esp), "r"(current_task->ebp), 
			"r"(current_task->pd[1023]&PAGE_MASK) : "eax");
	if(current_task->eip == globa)
	{
		post_context_switch();
		return;
	}
	asm("jmp *%0"::"r"(current_task->eip));
}

void check_alarms()
{
	task_t *t = alarm_list_start;
	task_critical();
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
			do_send_signal(t->pid, SIGALRM, 1);
		}
		t=t->alarm_next;
	}
	task_uncritical();
}
