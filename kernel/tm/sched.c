#include <sea/asm/system.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>

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
	/* threads that are in the kernel ignore signals until they're out of a syscall, in case
	 * there's an interrupt. A syscall that waits or blocks is responsible for checking signals
	 * manually. Kernel threads must do this as well if they want to handle signals. */
	if(current_thread->signals_pending && !current_thread->signal) {
		int index = 0;
		/* find the next signal in signals_pending and set signal to it */
		for(; index < 32; ++index) {
			if(current_thread->signals_pending & (1 << index))
				break;
		}
		if(index < 32) {
			current_thread->signal = index + 1;
			and_atomic(&current_thread->signals_pending, ~(1 << (index)));
		}
	}
	if(current_thread->signal) {
		if(current_thread->state == THREADSTATE_INTERRUPTIBLE)
			tm_thread_unblock(current_thread);
		if(!current_thread->system && current_thread->state == THREADSTATE_RUNNING)
			tm_thread_handle_signal(current_thread->signal);
	}

	tm_thread_lower_flag(current_thread, THREAD_SCHEDULE);
}

static void finish_schedule(void)
{
}

void tm_schedule(void)
{
	int old = cpu_interrupt_set(0);
	if(current_thread->interrupt_level)
		panic(PANIC_NOSYNC | PANIC_INSTANT, "tried to reschedule within interrupt context");
	assert(__current_cpu->preempt_disable >= 0);
	if(__current_cpu->preempt_disable > 0 || !(__current_cpu->flags & CPU_RUNNING)) {
		cpu_interrupt_set(old);
		return;
	}
	cpu_disable_preemption();
	prepare_schedule();
	struct thread *next = get_next_thread();

	if(current_thread != next) {
		addr_t jump = next->jump_point;
		next->jump_point = 0;
		if(!(next->stack_pointer > (addr_t)next->kernel_stack + sizeof(addr_t))) {
			panic(0, "kernel stack overrun! thread=%x:%d %x (min = %x)", next, next->tid, next->stack_pointer, next->kernel_stack);
		}
		cpu_set_kernel_stack(next->cpu, (addr_t)next->kernel_stack,
				(addr_t)next->kernel_stack + (KERN_STACK_SIZE));
		if(next->process != current_thread->process) {
			mm_vm_switch_context(&next->process->vmm_context);
		}

		arch_tm_thread_switch(current_thread, next, jump);
	}

	struct cpu *thiscpu = current_thread->cpu;
	finish_schedule();
	cpu_enable_preemption();
	cpu_interrupt_set(old);
}

