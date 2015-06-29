#include <sea/tm/thread.h>
#include <sea/tm/process.h>

static struct thread *get_next_thread()
{
	struct thread *n = tqueue_next(current_cpu->active_queue);
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
	if(kernel_state_flags & PREEMPT_DISABLED)
		return;
	int old = cpu_interrupt_set(0);
	prepare_schedule();
	struct thread *next = get_next_thread();

	arch_tm_thread_switch(current_thread, next);

	finish_schedule();
	cpu_interrupt_set(old);
}

