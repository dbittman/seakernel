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
	return m->lock;
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
	while(bts_atomic(&m->lock, 0)) {
		if(!(m->flags & MT_NOSCHED)) {
			cpu_enable_preemption();
			tm_schedule();
			cpu_disable_preemption();
		} else {
			cpu_pause();
		}
#if MUTEX_DEBUG
		if(--timeout == 0)
			panic(PANIC_NOSYNC | PANIC_INSTANT, "timeout locking mutex (owned by %d)", m->pid);
#endif
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
	btr_atomic(&m->lock, 0);
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
	m->lock=0;
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
