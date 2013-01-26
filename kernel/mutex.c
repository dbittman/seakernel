#include <atomic.h>
#include <mutex.h>
#include <kernel.h>
#include <task.h>

void mutex_acquire(mutex_t *m)
{
	if(m->lock && m->pid == (int)current_task->pid)
		panic(0, "task %d tried to relock mutex", m->pid);
	assert(m->magic == MUTEX_MAGIC);
	while(bts_atomic(&m->lock, 0))
		schedule();
	m->pid = current_task->pid;
}

void mutex_release(mutex_t *m)
{
	assert(m->lock);
	if(m->pid != (int)current_task->pid)
		panic(0, "task %d tried to release mutex it didn't own", m->pid);
	assert(m->magic == MUTEX_MAGIC);
	m->pid = -1;
	btr_atomic(&m->lock, 0);
}

mutex_t *mutex_create(mutex_t *m)
{
	if(!m) {
		m = (void *)kmalloc(sizeof(mutex_t));
		m->flags |= MT_ALLOC;
	} else
		m->flags=0;
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
