#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/tm/async_call.h>
#include <sea/tm/ticker.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>

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

void tm_thread_add_to_blocklist(struct thread *t, struct llist *blocklist)
{
	mutex_acquire(&t->block_mutex);
	assert(!t->blocklist);
	tqueue_remove(t->cpu->active_queue, &t->activenode);
	t->blocklist = blocklist;
	ll_do_insert(blocklist, &t->blocknode, (void *)t);
	mutex_release(&t->block_mutex);
}

void tm_thread_remove_from_blocklist(struct thread *t)
{
	mutex_acquire(&t->block_mutex);
	if(t->blocklist) {
		ll_do_remove(t->blocklist, &t->blocknode, 0);
		t->blocklist = 0;
		tqueue_insert(t->cpu->active_queue, (void *)t, &t->activenode);
	}
	mutex_release(&t->block_mutex);
}

int tm_thread_block(struct llist *blocklist, int state)
{
	cpu_disable_preemption();
	assert(!current_thread->blocklist);
	assert(state != THREADSTATE_RUNNING);
	int ret;
	if(state == THREADSTATE_INTERRUPTIBLE && (ret=tm_thread_got_signal(current_thread))) {
		cpu_enable_preemption();
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	tm_thread_add_to_blocklist(current_thread, blocklist);
	tm_thread_set_state(current_thread, state);
	cpu_enable_preemption();
	tm_schedule();
	if((ret=tm_thread_got_signal(current_thread)) && state != THREADSTATE_UNINTERRUPTIBLE) {
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	return 0;
}

int tm_thread_block_schedule_work(struct llist *blocklist, int state, struct async_call *work)
{
	cpu_disable_preemption();
	assert(!current_thread->blocklist);
	assert(state != THREADSTATE_RUNNING);
	int ret;
	if(state == THREADSTATE_INTERRUPTIBLE && (ret=tm_thread_got_signal(current_thread))) {
		cpu_enable_preemption();
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	tm_thread_add_to_blocklist(current_thread, blocklist);
	tm_thread_set_state(current_thread, state);
	workqueue_insert(&__current_cpu->work, work);
	assert(__current_cpu->preempt_disable == 1);
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

void tm_blocklist_wakeall(struct llist *blocklist)
{
	struct llistnode *node, *next;
	struct thread *t;
	rwlock_acquire(&blocklist->rwl, RWL_WRITER);
	ll_for_each_entry_safe(blocklist, node, next, struct thread *, t) {
		mutex_acquire(&t->block_mutex);
		if(t->blocklist) {
			ll_do_remove(t->blocklist, &t->blocknode, 1);
			t->blocklist = 0;
			tqueue_insert(t->cpu->active_queue, (void *)t, &t->activenode);
		}
		t->state = THREADSTATE_RUNNING;
		mutex_release(&t->block_mutex);
	}
	rwlock_release(&blocklist->rwl, RWL_WRITER);
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
	ticker_insert(&cpu->ticker, microseconds, call);
	cpu_put_current(cpu);
	tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
	int ret;
	if((ret=tm_thread_got_signal(current_thread))) {
		return ret == SA_RESTART ? -ERESTART : -EINTR;
	}
	return 0;
}

void tm_thread_delay_sleep(time_t microseconds)
{
	struct cpu *cpu = __current_cpu;
	uint64_t end = cpu->ticker.tick + microseconds;
	while(cpu->ticker.tick < end) {
		cpu_pause();
	}
}

int tm_thread_block_timeout(struct llist *blocklist, time_t microseconds)
{
	struct async_call *call = &current_thread->block_timeout;
	call->func = __timeout_expired;
	call->priority = ASYNC_CALL_PRIORITY_MEDIUM;
	call->data = (unsigned long)current_thread;
	cpu_disable_preemption();
	if(!tm_thread_got_signal(current_thread)) {
		struct ticker *ticker = &__current_cpu->ticker;
		ticker_insert(ticker, microseconds, call);
		tm_thread_add_to_blocklist(current_thread, blocklist);
		tm_thread_set_state(current_thread, THREADSTATE_INTERRUPTIBLE);
		cpu_enable_preemption();
		tm_schedule();
		cpu_disable_preemption();
		ticker_delete(&current_thread->cpu->ticker, call);
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

