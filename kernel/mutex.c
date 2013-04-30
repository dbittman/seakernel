/* mutex.c - Handles mutual exclusion locks 
 * copyright 2013 Daniel Bittman
 * 
 * These are much simpler than RWlocks. They only use 1 bit and can be
 * in only two states: locked or unlocked. 
 */
#include <atomic.h>
#include <mutex.h>
#include <kernel.h>
#include <task.h>

void __mutex_acquire(mutex_t *m, char *file, int line)
{
	/* are we re-locking ourselves? */
	if(current_task && m->lock && m->pid == (int)current_task->pid)
		panic(0, "task %d tried to relock mutex (%s:%d)", m->pid, file, line);
	assert(m->magic == MUTEX_MAGIC);
	/* wait until we can set bit 0. once this is done, we have the lock */
	//int t = 1000000;
	while(bts_atomic(&m->lock, 0)) {
		if(!(m->flags & MT_NOSCHED))
			schedule();
		else
			asm("pause"); /* the intel manuals suggest this */
		//if(!--t) panic(0, "mutex timeout (%s:%d)", file, line);
	}
	if(current_task) m->pid = current_task->pid;
}

void __mutex_release(mutex_t *m, char *file, int line)
{
	assert(m->lock);
	if(current_task && m->pid != (int)current_task->pid)
		panic(0, "task %d tried to release mutex it didn't own (%s:%d)", m->pid, file, line);
	assert(m->magic == MUTEX_MAGIC);
	m->pid = -1;
	btr_atomic(&m->lock, 0);
}

mutex_t *mutex_create(mutex_t *m, unsigned flags)
{
	if(!m) {
		m = (void *)kmalloc(sizeof(mutex_t));
		m->flags |= (MT_ALLOC | flags);
	} else
		m->flags=flags;
	m->lock=0;
	m->magic = MUTEX_MAGIC;
	return m;
}

void mutex_destroy(mutex_t *m)
{
	assert(m->magic == MUTEX_MAGIC);
	m->lock = m->magic = 0;
	if(m->flags & MT_ALLOC)
		kfree(m);
}
