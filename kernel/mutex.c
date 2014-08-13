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
#include <sea/tm/schedule.h>
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

void __mutex_acquire(mutex_t *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	/* are we re-locking ourselves? */
	if(current_task && m->lock && ((m->pid == (int)current_task->pid) && ((m->lock & MT_LCK_INT) || !(current_task->flags & TF_IN_INT))))
		panic(0, "task %d tried to relock mutex %x (%s:%d)", m->pid, m->lock, file, line);	
	/* check for a potential deadlock */
	if(current_task
#if CONFIG_SMP
		&& !(kernel_state_flags & KSF_SMP_ENABLE)
#endif
		&& (m->flags & MT_NOSCHED) && !(current_task->cpu->flags&CPU_INTER)
		&& (int)current_task->pid != m->pid && m->pid != -1)
			panic(0, "mutex will deadlock (%d %d): %s:%d\n", file, line);
#if CONFIG_DEBUG
	int t = 100000000;
#endif
	if(current_task && (m->pid == (int)current_task->pid) && (current_task->flags & TF_IN_INT)) {
		/* we don't need to be atomic, since we already own the lock */
		assert(!(m->lock & MT_LCK_INT));
		m->lock |= MT_LCK_INT;
		return;
	}
	/* wait until we can set bit 0. once this is done, we have the lock */
	while(bts_atomic(&m->lock, 0)) {
		if(!(m->flags & MT_NOSCHED))
			tm_schedule();
		else
			arch_cpu_pause();
#if CONFIG_DEBUG
		if(!--t) panic(0, "mutex time out %s:%d\n", file, line);
#endif
	}
	assert(m->lock);
	if(current_task) m->pid = current_task->pid;
}

void __mutex_release(mutex_t *m, char *file, int line)
{
	assert(m->magic == MUTEX_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	assert(m->lock);
	if(current_task && m->pid != (int)current_task->pid)
		panic(0, "task %d tried to release mutex it didn't own (%s:%d)", m->pid, file, line);
	if(m->lock & MT_LCK_INT)
	{
		assert(current_task->flags & TF_IN_INT);
		m->lock &= ~MT_LCK_INT;
		return;
	}
	m->pid = -1;
	btr_atomic(&m->lock, 0);
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
