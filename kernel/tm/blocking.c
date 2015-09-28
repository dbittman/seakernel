#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/tm/async_call.h>
#include <sea/tm/ticker.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>

#include <sea/errno.h>
#include <sea/tm/timing.h>

/* a thread may not block another thread. Only a thread may block itself.
 * However, ANY thread may unblock another thread. */
void tm_thread_set_state(struct thread *t, int state)
{
	int oldstate = t->state;
	t->state = state;
	if(oldstate != state && t == current_thread && t->interrupt_level == 0)
		tm_schedule();
	else
		tm_thread_raise_flag(t, THREAD_SCHEDULE);
}

/* used to wake a thread that is either sleeping or nearly sleeping */
void tm_thread_poke(struct thread *t)
{
	tm_thread_raise_flag(t, THREAD_WAKEUP);
	tm_thread_unblock(t);
}

void tm_thread_add_to_blocklist(struct linkedlist *blocklist)
{
	/* we are the only ones that may add ourselves to a blocklist.
	 * Thus we know that if we get here, current_thread->blocklist
	 * must be null until WE set it otherwise.
	 * Additionally, we require preemption to be OFF before calling this. */
	assert(blocklist);
	assert(!current_thread->blocklist);
	assert(__current_cpu->preempt_disable > 0);
	tqueue_remove(__current_cpu->active_queue, &current_thread->activenode);
	atomic_store(&current_thread->blocklist, blocklist);
	linkedlist_insert(blocklist, &current_thread->blocknode, (void *)current_thread);
}

void tm_thread_remove_from_blocklist(struct thread *t)
{
	struct linkedlist *bl = atomic_exchange(&t->blocklist, NULL);
	if(bl) {
		linkedlist_remove(bl, &t->blocknode);
		tqueue_insert(t->cpu->active_queue, (void *)t, &t->activenode);
	}
}

int tm_thread_block(struct linkedlist *blocklist, int state)
{
	cpu_disable_preemption();
	assert(__current_cpu->preempt_disable == 1);
	assert(!current_thread->blocklist);
	assert(state != THREADSTATE_RUNNING);
	int ret;
	if(state == THREADSTATE_INTERRUPTIBLE && (ret=tm_thread_got_signal(current_thread))) {
		cpu_enable_preemption();
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	tm_thread_set_state(current_thread, state);
	tm_thread_add_to_blocklist(blocklist);
	cpu_enable_preemption();
	tm_schedule();
	if((ret=tm_thread_got_signal(current_thread)) && state != THREADSTATE_UNINTERRUPTIBLE) {
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	return 0;
}

int tm_thread_block_schedule_work(struct linkedlist *blocklist, int state, struct async_call *work)
{
	cpu_disable_preemption();
	assert(__current_cpu->preempt_disable == 1);
	assert(!current_thread->blocklist);
	assert(state != THREADSTATE_RUNNING);
	int ret;
	if(state == THREADSTATE_INTERRUPTIBLE && (ret=tm_thread_got_signal(current_thread))) {
		cpu_enable_preemption();
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	current_thread->state = state;
	tm_thread_add_to_blocklist(blocklist);
	workqueue_insert(&__current_cpu->work, work);
	cpu_enable_preemption();
	tm_schedule();
	if((ret=tm_thread_got_signal(current_thread)) && state != THREADSTATE_UNINTERRUPTIBLE) {
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	return 0;
}

void tm_thread_unblock(struct thread *t)
{
	tm_thread_set_state(t, THREADSTATE_RUNNING);
	tm_thread_remove_from_blocklist(t);
}

static bool __do_wakeup(struct linkedentry *entry)
{
	struct thread *t = entry->obj;
	struct llist *bl = atomic_exchange(&t->blocklist, NULL);
	if(bl) {
		tqueue_insert(t->cpu->active_queue, (void *)t, &t->activenode);
		t->state = THREADSTATE_RUNNING;
		return true;
	}
	t->state = THREADSTATE_RUNNING;
	return false;
}

void tm_blocklist_wakeall(struct linkedlist *blocklist)
{
	struct llistnode *node, *next;
	struct thread *t;
	linkedlist_apply(blocklist, __do_wakeup);
}

static void __timeout_expired(unsigned long data)
{
	struct thread *t = (struct thread *)data;
	if(t->blocklist) {
		tm_thread_remove_from_blocklist(t);
		tm_thread_raise_flag(t, THREAD_TIMEOUT_EXPIRED);
	}
	tm_thread_set_state(t, THREADSTATE_RUNNING);
}

int tm_thread_delay(time_t microseconds)
{
	struct async_call *call = &current_thread->block_timeout;
	call->func = __timeout_expired;
	call->priority = ASYNC_CALL_PRIORITY_MEDIUM;
	call->data = (unsigned long)current_thread;
	struct cpu *cpu = cpu_get_current();
	struct ticker *ticker = &cpu->ticker;
	ticker_insert(ticker, microseconds, call);
	cpu_put_current(cpu);
	tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
	int old = cpu_interrupt_set(0);
	ticker_delete(ticker, call);
	cpu_interrupt_set(old);
	int ret;
	if((ret=tm_thread_got_signal(current_thread))) {
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	return 0;
}

int sys_delay(long time)
{
	time_t start = tm_timing_get_microseconds();
	switch(tm_thread_delay(time * ONE_MILLISECOND)) {
		signed long remaining = 0;
		case -ERESTART:
			return -ERESTART;
		case -EINTR:
			remaining = time - (tm_timing_get_microseconds() - start) / ONE_MILLISECOND;
			if(remaining <= 0)
				return 0;
			return remaining;
	}
	return 0;
}

void tm_thread_delay_sleep(time_t microseconds)
{
	struct cpu *cpu = __current_cpu;
	uint64_t end = cpu->ticker.tick + microseconds;
	while(cpu->ticker.tick < end) {
		cpu_pause();
		/* force the compiler to load the value of tick on each iteration */
		atomic_thread_fence(memory_order_release);
	}
}

int tm_thread_block_timeout(struct linkedlist *blocklist, time_t microseconds)
{
	struct async_call *call = &current_thread->block_timeout;
	call->func = __timeout_expired;
	call->priority = ASYNC_CALL_PRIORITY_MEDIUM;
	call->data = (unsigned long)current_thread;
	cpu_disable_preemption();
	assert(__current_cpu->preempt_disable == 1);
	if(!tm_thread_got_signal(current_thread)) {
		struct ticker *ticker = &__current_cpu->ticker;
		ticker_insert(ticker, microseconds, call);
		tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
		tm_thread_add_to_blocklist(blocklist);
		cpu_enable_preemption();
		tm_schedule();
		int old = cpu_interrupt_set(0);
		cpu_disable_preemption();
		ticker_delete(&current_thread->cpu->ticker, call);
		cpu_interrupt_set(old);
	}
	if(current_thread->flags & THREAD_TIMEOUT_EXPIRED) {
		tm_thread_lower_flag(current_thread, THREAD_TIMEOUT_EXPIRED);
		cpu_enable_preemption();
		return -ETIME;
	}
	int ret;
	if((ret=tm_thread_got_signal(current_thread))) {
		cpu_enable_preemption();
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	cpu_enable_preemption();
	return 0;
}

