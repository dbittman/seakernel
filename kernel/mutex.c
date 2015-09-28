/* mutex.c - Handles mutual exclusion locks
 * copyright 2013 Daniel Bittman
 *
 * These are much simpler than RWlocks. They only use 1 bit and can be
 * in only two states: locked or unlocked.
 */

#include <sea/mutex.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/asm/system.h>
#include <sea/mm/kmalloc.h>
/* a task may relock a mutex if it is inside an interrupt handler,
 * and has previously locked the same mutex outside of the interrupt
 * handler. this allows for a task to handle an event that requires
 * a mutex to be locked in the handler whilst having locked the mutex
 * previously */

int mutex_is_locked(mutex_t *m)
{
	/* this is ONLY TO BE USED AS A HINT. The mutex state MAY change
	 * before this information is used, or even DURING THE RETURN of
	 * this function */
	if(kernel_state_flags & KSF_SHUTDOWN)
		return 1;
	return atomic_load(&m->lock);
}
#define MUTEX_DEBUG 0
/* TODO: split MT_NOSCHED into struct spinlock */
void __mutex_acquire(mutex_t *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);

	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot lock a mutex within interrupt context (%s:%d)", file, line);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	/* are we re-locking ourselves? */
	if(current_thread && m->lock && ((m->pid == (pid_t)current_thread->tid)))
		panic(0, "task %d tried to relock mutex %x (%s:%d)", m->pid, m->lock, file, line);
	/* wait until we can set bit 0. once this is done, we have the lock */
#if MUTEX_DEBUG
	int timeout = 8000;
#endif
	if(current_thread && __current_cpu->preempt_disable > 0)
		panic(0, "tried to lock schedulable mutex with preempt off");

	int unlocked = 0;
	int locked = 1;

	if(current_thread)
		current_thread->held_locks++;

	/* success is given memory_order_acq_rel because the loop body will not run, but
	 * we mustn't let any memory accesses bubble up above the exchange, and we want
	 * held_locks to not bubble down. fail is given
	 * memory_order_acquire because we musn't let the resetting of unlocked
	 * bubble up anywhere. */
	while(!atomic_compare_exchange_weak_explicit(&m->lock, &unlocked, locked, memory_order_acq_rel, memory_order_acquire)) {
		unlocked = 0;
		/* we can use __current_cpu here, because we're testing if we're the idle
		 * thread, and the idle thread never migrates. */
		if(current_thread && current_thread != __current_cpu->idle_thread) {
			printk_safe(0, "thread %d blocking from %s:%d\n", current_thread->tid,
					file, line);
			tm_thread_block(&m->blocklist, THREADSTATE_UNINTERRUPTIBLE);
		} else if(current_thread) {
			tm_schedule();
		}
#if MUTEX_DEBUG
		if(--timeout == 0) {
			panic(0, "mutex timeout from %s:%d (owned by %d: %s:%d)\n", file, line, m->pid, m->owner_file, m->owner_line);
		}
#endif
	}
	assert(m->lock);
	if(current_thread) m->pid = current_thread->tid;
	m->owner_file = file;
	m->owner_line = line;
}

void __mutex_release(mutex_t *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	assert(m->lock);
	if(current_thread && m->pid != (int)current_thread->tid)
		panic(0, "task %d tried to release mutex it didn't own (%s:%d)", m->pid, file, line);
	m->pid = -1;
	m->owner_file = 0;
	m->owner_line = 0;
	/* must be memory_order_release because we don't want m->pid to bubble-down below
	 * this line */
	atomic_store(&m->lock, 0);
	if(current_thread) {
		tm_blocklist_wakeone(&m->blocklist);
	}
	if(current_thread)
		current_thread->held_locks--;
}

mutex_t *mutex_create(mutex_t *m, unsigned flags)
{
	if(!m) {
		m = (void *)kmalloc(sizeof(mutex_t));
		m->flags |= (MT_ALLOC | flags);
	} else {
		memset(m, 0, sizeof(mutex_t));
		m->flags=flags;
	}
	m->lock=ATOMIC_VAR_INIT(0);
	m->magic = MUTEX_MAGIC;
	m->pid = -1;
	linkedlist_create(&m->blocklist, 0);
	return m;
}

void mutex_destroy(mutex_t *m)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	if(m->lock && current_thread && m->pid == current_thread->tid)
		current_thread->held_locks--;
	m->lock = m->magic = 0;
	linkedlist_destroy(&m->blocklist);
	if(m->flags & MT_ALLOC)
		kfree(m);
}
