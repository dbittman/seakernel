#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/tm/async_call.h>

/* TODO: need to handle EINTR */
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
	
}

void tm_thread_remove_from_blocklist(struct thread *t)
{

}

int tm_thread_block(struct thread *t, struct llist *blocklist, int state)
{
	tm_thread_add_to_blocklist(t, blocklist);
	tm_thread_set_state(t, state);
	if(t->signal && state != TASK_UNINTERRUPTIBLE) {
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

