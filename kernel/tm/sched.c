#include <sea/asm/system.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <stdatomic.h>
#include <sea/vsprintf.h>
#include <sea/lib/timer.h>
static void check_signals(struct thread *thread)
{
	assert(thread);
	if(thread->signals_pending && !thread->signal && !(thread->flags & THREAD_SIGNALED)) {
		int index = 0;
		/* find the next signal in signals_pending and set signal to it */
		for(; index < 32; ++index) {
			if(thread->signals_pending & (1 << index)
					&& !(thread->process->global_sig_mask & (1 << (index + 1))))
				break;
		}
		if(index < 32) {
			thread->signal = index + 1;
			atomic_fetch_and_explicit(&thread->signals_pending,
					~(1 << (index)), memory_order_relaxed);
		}
	}
	if(thread->signal || thread->signals_pending) {
		if(thread->state == THREADSTATE_INTERRUPTIBLE)
			tm_thread_unblock(thread);
	}
}

static struct thread *get_next_thread (void)
{
	struct thread *n = 0;
	while(1) {
		n = tqueue_next(current_thread->cpu->active_queue);
		assert(n && n->cpu == current_thread->cpu);
		check_signals(n);
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
	if(unlikely(current_thread->signal)) {
		if(current_thread->state == THREADSTATE_INTERRUPTIBLE)
			tm_thread_unblock(current_thread);
		/* NOTE: before we checked thread state, there was a bug where a thread that
		 * called exit and got a signal during the exit would exit twice. This is because
		 * it would finish the syscall and then reschedule, which would then handle the signal
		 * here, which would then call exit...again. */
		if(!current_thread->system && current_thread->state == THREADSTATE_RUNNING && !(current_thread->flags & THREAD_SIGNALED))
			tm_thread_handle_signal(current_thread->signal);
	}
	if(unlikely(current_thread->flags & THREAD_WAKEUP)) {
		/* wake-up says "next time we get set to interruptible, ignore it and
		 * restart". */
		if(current_thread->state == THREADSTATE_INTERRUPTIBLE) {
			tm_thread_unblock(current_thread);
			tm_thread_lower_flag(current_thread, THREAD_WAKEUP);
		}
	}
	tm_thread_lower_flag(current_thread, THREAD_SCHEDULE);
}

#include <sea/tm/timing.h>
static void post_schedule(void)
{
	struct workqueue *wq = &__current_cpu->work;
	if(wq->count > 0 && !current_thread->held_locks) {
		if(wq->count > 30 && ((tm_timing_get_microseconds() / 100000) % 10) == 0) {
			printk(0, "[sched]: cpu %d: warning - work is piling up (%d tasks)!\n",
					__current_cpu->knum, wq->count);
		}
		/* if work is piling up, clear out tasks */
		//do {
			workqueue_dowork(wq);
			//atomic_thread_fence(memory_order_seq_cst);
		//} while(wq->count >= 30);
	}
	if(unlikely(current_thread->resume_work.count)) {
		while(workqueue_dowork(&current_thread->resume_work) != -1) {
			tm_schedule();
		}
	}
}

void tm_schedule(void)
{
	int old = cpu_interrupt_set(0);
	if(current_thread->interrupt_level)
		panic(PANIC_NOSYNC | PANIC_INSTANT,
				"tried to reschedule within interrupt context (%d)",
				current_thread->interrupt_level);
	assert(__current_cpu->preempt_disable >= 0);
	if(__current_cpu->preempt_disable > 0 || !(__current_cpu->flags & CPU_RUNNING)) {
		cpu_interrupt_set(old);
		return;
	}
	cpu_disable_preemption();
	prepare_schedule();
	struct thread *next = get_next_thread();

	if(current_thread != next) {
		/* save this somewhere that's not on the stack so that it still is correct
		 * after a context switch */
		addr_t jump = next->jump_point;
		next->jump_point = 0;
		assertmsg(next->stack_pointer > (addr_t)next->kernel_stack + sizeof(addr_t),
					"kernel stack overrun! thread=%x:%d %x (min = %x)",
					next, next->tid, next->stack_pointer, next->kernel_stack);
		/* if the thread state is dead, and we're scheduling away from it, then
		 * it's finished exiting and is waiting for cleanup. This is okay! Since
		 * we require that the only places where we do work for this CPU's workqueue
		 * is in it's idle thread or inside the scheduler for that that CPU, we can
		 * ensure that work from the workqueue won't be done while that CPU is scheduling,
		 * so the thread can't be released after this statement, until it has totally
		 * scheduled away.
		 */
		if(unlikely(current_thread->state == THREADSTATE_DEAD)) {
			tm_thread_raise_flag(current_thread, THREAD_DEAD);
		}
		cpu_set_kernel_stack(next->cpu, (addr_t)next->kernel_stack,
				(addr_t)next->kernel_stack + (KERN_STACK_SIZE));
		arch_tm_thread_switch(current_thread, next, jump);
	}

	cpu_enable_preemption();
	cpu_interrupt_set(old);
	post_schedule();
}

