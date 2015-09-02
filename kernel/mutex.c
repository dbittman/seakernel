/* mutex.c - Handles mutual exclusion locks
 * copyright 2013 Daniel Bittman
 *
 * These are much simpler than RWlocks. They only use 1 bit and can be
 * in only two states: locked or unlocked.
 */
#include <sea/cpu/atomic.h>
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
void __mutex_acquire(mutex_t *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);

	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(current_thread && current_thread->interrupt_level && !(m->flags & MT_NOSCHED))
		panic(PANIC_NOSYNC, "cannot lock a normal mutex within interrupt context (%s:%d)", file, line);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	/* are we re-locking ourselves? */
	if(current_thread && m->lock && ((m->pid == (pid_t)current_thread->tid)))
		panic(0, "task %d tried to relock mutex %x (%s:%d)", m->pid, m->lock, file, line);
	/* wait until we can set bit 0. once this is done, we have the lock */
#if MUTEX_DEBUG
	int timeout = 800000000;
#endif
	cpu_disable_preemption();

	int unlocked = 0;
	int locked = 1;
	/* success is given memory_order_acquire because the loop body will not run, but
	 * we mustn't let any memory accesses bubble up above the exchange. fail is given
	 * memory_order_acquire because we musn't let the resetting of unlocked
	 * bubble up anywhere. */
	while(!atomic_compare_exchange_weak_explicit(&m->lock, &unlocked, locked, memory_order_acquire, memory_order_acquire)) {
		unlocked = 0;
		if(!(m->flags & MT_NOSCHED)) {
			cpu_enable_preemption();
			tm_schedule();
			cpu_disable_preemption();
		} else {
			cpu_pause();
		}
	}
	assert(m->lock);
	if(!(m->flags & MT_NOSCHED))
		cpu_enable_preemption();
	if(current_thread) m->pid = current_thread->tid;
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
	/* must be memory_order_release because we don't want m->pid to bubble-down below
	 * this line */
	atomic_store_explicit(&m->lock, 0, memory_order_release);
	if(m->flags & MT_NOSCHED)
		cpu_enable_preemption();
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
	return m;
}

void mutex_destroy(mutex_t *m)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	m->lock = m->magic = 0;
	if(m->flags & MT_ALLOC)
		kfree(m);
}
