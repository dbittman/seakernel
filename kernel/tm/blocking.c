#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/tm/async_call.h>

/* TODO: need to handle EINTR */
/* TODO: this doesn't handle the situation where we block / unblock a task on a different
 * CPU... that's not safe here */
void tm_thread_set_state(struct thread *t, int state)
{
	int oldstate = t->state;
	t->state = state;
	/* TODO: What if this process is running on a different CPU */
	if(oldstate != state && t == current_thread)
		tm_schedule();
}

void tm_thread_add_to_blocklist(struct thread *t, struct llist *blocklist)
{
	int old = cpu_interrupt_set(0);
	task->blocklist = list;
	ll_do_insert(list, task->blocknode, (void *)task);
	tqueue_remove(task->cpu->active_queue, task->activenode);
	cpu_interrupt_set(old);
}

void tm_thread_remove_from_blocklist(struct thread *t)
{
	int old = cpu_interrupt_set(0);
	tqueue_insert(t->cpu->active_queue, (void *)t, t->activenode);
	struct llistnode *bn = t->blocknode;
	t->blocklist = 0;
	ll_do_remove(list, bn, 0);
	cpu_interrupt_set(old);
}

int tm_thread_block(struct thread *t, struct llist *blocklist, int state)
{
	int old = cpu_interrupt_set(0);
	tm_thread_add_to_blocklist(t, blocklist);
	tm_thread_set_state(t, state);
	cpu_interrupt_set(old);
	if(tm_thread_got_signal(t) && state != TASK_UNINTERRUPTIBLE) {
		return -EINTR;
	}
	return 0;
}

void tm_thread_unblock(struct thread *t)
{

}

void tm_blocklist_wakeall(struct llist *blocklist)
{

}

static void __timeout_expired(unsigned long data)
{
	struct thread *t = (struct thread *)data;
	tm_thread_remove_from_blocklist(t);
	raise_flag(t, TF_TIMEOUT_EXPIRED);
	tm_thread_set_state(t, TASK_RUNNING); /* TODO: maybe restore an old state */
}

int tm_thread_delay(struct thread *thread, time_t nanoseconds)
{
	struct async_call *call = &thread->block_timeout;
	call->func = __timeout_expired;
	call->priority = 10; /* TODO: what priority */
	call->data = (unsigned long)thread;
	ticker_insert(SOME_TICKER, nanoseconds, call);
	tm_thread_set_state(thread, TASK_INTERRUPTIBLE);
	if(tm_thread_got_signal(thread))
		return -EINTR;
	return 0;
}

int tm_thread_block_timeout(struct thread *thread, struct llist *blocklist, time_t nanoseconds)
{
	struct async_call *call = &thread->block_timeout;
	call->func = __timeout_expired;
	call->priority = 10; /* TODO: what priority */
	call->data = (unsigned long)thread;
	ticker_insert(SOME_TICKER, nanoseconds, call);
	int r = tm_thread_block(t, blocklist);
	/* TODO: do we care more about EINTR or ETIME? */
	if(test_flag(t, TF_TIMEOUT_EXPIRED)) {
		lower_flag(t, TF_TIMEOUT_EXPIRED); /* TODO: do the test and set atomically */
		return -ETIME;
	}
	return r;
}

