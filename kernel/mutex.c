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
#include <sea/tm/blocking.h>
#include <sea/asm/system.h>
#include <sea/mm/kmalloc.h>

static bool __confirm(void *data)
{
	struct mutex *m = data;
	if(!atomic_load(&m->lock))
		return false;
	return true;
}

#define MUTEX_DEBUG 0
void __mutex_acquire(struct mutex *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);

	if(unlikely(kernel_state_flags & KSF_DEBUGGING))
		return;
	if(unlikely(current_thread && current_thread->interrupt_level))
		panic(PANIC_NOSYNC, "cannot lock a mutex within interrupt context (%s:%d)", file, line);
	if(unlikely(kernel_state_flags & KSF_SHUTDOWN)) return;
	/* wait until we can set bit 0. once this is done, we have the lock */
#if MUTEX_DEBUG
	int timeout = 8000;
#endif
	if(unlikely(current_thread && __current_cpu->preempt_disable > 0))
		panic(0, "tried to lock schedulable mutex with preempt off");

	if(likely(current_thread != NULL))
		current_thread->held_locks++;

	while(atomic_exchange(&m->lock, true)) {
		if(likely(current_thread != NULL)) {
			/* are we re-locking ourselves? */
			if(m->owner == current_thread)
				panic(0, "tried to relock mutex (%s:%d)",
						file, line);
			/* we can use __current_cpu here, because we're testing if we're the idle
		 	 * thread, and the idle thread never migrates. */
			if(current_thread != __current_cpu->idle_thread) {
				tm_thread_block_confirm(&m->blocklist, THREADSTATE_UNINTERRUPTIBLE,
						__confirm, m);
			} else {
				tm_schedule();
			}
		}
#if MUTEX_DEBUG
		if(--timeout == 0) {
			panic(0, "mutex timeout from %s:%d (owned by %d: %s:%d)\n", file, line, m->pid, m->owner_file, m->owner_line);
		}
#endif
	}
	m->owner = current_thread;
	m->owner_file = file;
	m->owner_line = line;
}

void __mutex_release(struct mutex *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	if(m->owner != current_thread)
		panic(0, "task %d tried to release mutex it didn't own (%s:%d)", current_thread->tid, file, line);
	m->owner = NULL;
	m->owner_file = 0;
	m->owner_line = 0;
	/* must be memory_order_release because we don't want m->pid to bubble-down below
	 * this line */
	atomic_store(&m->lock, false);
	if(current_thread) {
		tm_blocklist_wakeone(&m->blocklist);
	}
	if(current_thread)
		current_thread->held_locks--;
}

struct mutex *mutex_create(struct mutex *m, unsigned flags)
{
	if(!m) {
		m = (void *)kmalloc(sizeof(struct mutex));
		m->flags |= (MT_ALLOC | flags);
	} else {
		memset(m, 0, sizeof(struct mutex));
		m->flags=flags;
	}
	m->lock = ATOMIC_VAR_INIT(0);
	m->magic = MUTEX_MAGIC;
	blocklist_create(&m->blocklist, 0, "mutex");
	return m;
}

void mutex_destroy(struct mutex *m)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	if(m->lock && current_thread && m->owner == current_thread)
		current_thread->held_locks--;
	m->magic = 0;
	atomic_store(&m->lock, false);
	blocklist_destroy(&m->blocklist);
	if(m->flags & MT_ALLOC)
		kfree(m);
}

