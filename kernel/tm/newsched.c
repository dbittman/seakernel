#include <sea/asm/system.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
static void arch_tm_thread_switch(struct thread *old, struct thread *new) 
{
	assert(new->stack_pointer > (addr_t)new->kernel_stack + sizeof(addr_t));
	tm_set_kernel_stack(new->cpu, new->kernel_stack,
			new->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	if(new->process != old->process) {
		mm_vm_switch_context(&new->process->vmm_context);
	}
	asm (
		"pushf;"
		"push %%ebx;"
		"push %%esi;"
		"push %%edi;"
		"push %%ebp;"
		"mov %%esp, %0;": "=r"(old->stack_pointer)::"memory");
	if(new->jump_point) {
		/* newly created thread, needs to just have some basic context set
		 * up initially and then jumped to */
		addr_t jump = new->jump_point;
		new->jump_point = 0;
		asm ("mov %1, %%ecx;"
				"mov %0, %%esp;"
				"pop %%ebp;"
				"jmp *%%ecx"::"r"(new->stack_pointer), "r"(jump):"memory");
	}
	asm ("mov %0, %%esp;"
			"pop %%ebp;"
			"pop %%edi;"
			"pop %%esi;"
			"pop %%ebx;"
			"popf"::"r"(new->stack_pointer):"memory");
	/* WARNING - we've switched stacks at this point! We must NOT use anything
	 * stack related now until this function returns! */
}

static struct thread *get_next_thread (void)
{
	struct thread *n = 0;
	while(1) {
		n = tqueue_next(current_thread->cpu->active_queue);
		assert(n->cpu == current_thread->cpu);
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

static void prepare_schedule(void)
{
	/* store arch-indep context */
	tm_thread_lower_flag(current_thread, TF_SCHED);
}

static void finish_schedule(void)
{
	/* restore arch-indep context */
	/* check signals */
	if(current_thread->signal && !current_thread->system)
		tm_thread_handle_signal(current_thread->signal);
}

void tm_schedule(void)
{
	int old = cpu_interrupt_set(0);
	if(__current_cpu->preempt_disable > 0) {
		cpu_interrupt_set(old);
		return;
	}
	cpu_disable_preemption();
	prepare_schedule();
	struct thread *next = get_next_thread();

	if(current_thread != next) {
		arch_tm_thread_switch(current_thread, next);
	}

	struct cpu *thiscpu = current_thread->cpu;
	finish_schedule();
	cpu_enable_preemption();
	cpu_interrupt_set(old);
	if(thiscpu->work.count > 0)
		workqueue_dowork(&thiscpu->work);
}

