#include <sea/asm/system.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
static void arch_tm_thread_switch(struct thread *old, struct thread *new)
{
	asm (
		"pushf;"
		"push %%ebx;"
		"push %%esi;"
		"push %%edi;"
		"push %%ebp;"
		"mov %%esp, %0;": "=r"(old->stack_pointer)::"memory");
	if(new->process->mm_context != old->process->mm_context) {
		asm ("mov %0, %%cr3;"::"r"(new->process->mm_context):"memory");
	}
	asm ("mov %0, %%esp;"
			"pop %%ebp;"
			"pop %%edi;"
			"pop %%esi;"
			"pop %%ebp;"
			"popf"::"r"(new->stack_pointer):"memory");
}

static struct thread *get_next_thread()
{
	struct thread *n = 0;
	while(1) {
		n = tqueue_next(current_thread->cpu->active_queue);
		if(n && tm_thread_runnable(n))
			break;
		if(!n || n == current_thread) {
			n = current_thread->cpu->idle_thread;
			break;
		}
	}
	assert(n && tm_thread_runnable(n));
	return n;
}

static void prepare_schedule()
{
	/* store arch-indep context */
}

static void finish_schedule()
{
	/* restore arch-indep context */
	/* check signals */
}

void tm_schedule()
{
	/* TODO: global preempt disable, or just on this CPU? */
	int old = cpu_interrupt_set(0);
	if(__current_cpu->flags & CPU_DISABLE_PREEMPT) {
		cpu_interrupt_set(old);
		return;
	}
	/* TODO: do we need to call a preempt_disable function */
	prepare_schedule();
	struct thread *next = get_next_thread();

	if(current_thread != next) {
		arch_tm_thread_switch(current_thread, next);
	}

	finish_schedule();
	cpu_interrupt_set(old);
}

