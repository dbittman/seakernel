/* mutex.c: Copyright (c) 2010 Daniel Bittman. 
 * Defines functions for mutual exclusion
 * Before using a mutex, use create_mutex. To gain exclusive access, 
 * use mutex_on. To release, mutex_off
 * 
 * Mutexes may not be kept outside of system calls, nor may they be 
 * kept during USLEEP and ISLEEP states.
 * The exception is that mutexes may be kept over wait_flag calls, 
 * however this must be done with extreme
 * caution, as this could easily lead to a deadlock.
 */
#include <kernel.h>
#include <mutex.h>
#include <memory.h>
#include <task.h>
#define MUTEX_DO_NOLOCK 0
__attribute__((optimize("O0"))) static inline int A_inc_mutex_count(mutex_t *m)
{
	asm("cli;lock incl %0":"+m"(m->count));
	return m->count;
}

__attribute__((optimize("O0"))) static inline int A_dec_mutex_count(mutex_t *m)
{
	asm("cli;lock decl %0":"+m" (m->count));
	return m->count;
}

__attribute__((optimize("O0"))) void reset_mutex(mutex_t *m)
{
	__super_cli();
	m->pid=-1;
	m->owner=m->count=0;
	m->magic = MUTEX_MAGIC;
	*m->file=0;
}

__attribute__((optimize("O0"))) static inline void engage_full_system_lock()
{
	__super_cli();
	task_critical();
	raise_flag(TF_LOCK);
}

__attribute__((optimize("O0"))) static inline void disengage_full_system_lock()
{
	task_full_uncritical();
	lower_flag(TF_LOCK);
}

mutex_t *create_mutex(mutex_t *existing)
{
	mutex_t *m=0;
	if(!existing) {
		m = (mutex_t *)kmalloc(sizeof(mutex_t));
		m->flags |= MF_ALLOC | MF_REAL;
	}
	else {
		m = existing;
		m->flags = MF_REAL;
	}
	m->magic = MUTEX_MAGIC;
	reset_mutex(m);
	return m;
}

/* Void because we must garuntee that this succeeds or panics */
__attribute__((optimize("O0"))) void __mutex_on(mutex_t *m, 
	char *file, int line)
{
	if(!m) return;
	if(!current_task || panicing) return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "mutex_on got invalid mutex: %x (%x): %s:%d\n", m, 
					m->magic, file, line);
	int i=0;
	char lock_was_raised = (current_task->flags & TF_LOCK) ? 1 : 0;
	engage_full_system_lock();
	//if(m->count) kprintf("double lock: %s:%d\n", file, line);
	/* This must be garunteed to only break when the mutex is free. */
	while(m->count > 0 && (unsigned)m->pid != current_task->pid) {
#if 1
		assert(m->pid != -1);
		task_t *t = (task_t *)m->owner;
		task_t *p = 0;//get_task_pid(m->pid);
		if(p && (p != t || m->pid != (int)p->pid || m->pid != (int)t->pid))
			panic(0, "Mutex confusion (%d)! \nm %d, t %d (%d:%d:%d), p %d; %s:%d\nWas set on: %s:%d", 
					current_task->pid, 
					m->pid, t->pid, t->state, t->system, t->flags, p->pid, 
					file, line, m->file, m->line);
#endif
		raise_flag(TF_WMUTEX);
		disengage_full_system_lock();
		/* Sleep...*/
		if(i++ == 1000)
			i=0, printk(0, "[mutex]: potential deadlock (%s:%d):\n\ttask %d (%d) is waiting for a mutex (p=%d,c=%d): %x\n", 
						file, line, current_task->pid, 
						current_task->system, m->pid, m->count, p);
		force_schedule();
		engage_full_system_lock();
	}
	assert(!m->count || (current_task->pid == (unsigned)m->pid));
	assert(m->count < MUTEX_COUNT);
	/* Here we update the mutex fields */
	raise_flag(TF_DIDLOCK);
	lower_flag(TF_WMUTEX);
#if 0
	strcpy((char *)m->file, file);
	m->line = line;
#endif
	/* update the fields */
	m->pid = current_task->pid;
	m->owner = (unsigned)current_task;
	A_inc_mutex_count(m);
	/* Clear up locks and return */
	if(lock_was_raised) {
		raise_flag(TF_LOCK);
		task_full_uncritical();
	}
	else {
		disengage_full_system_lock();
		__super_sti();
	}
}

__attribute__((optimize("O0"))) void __mutex_off(mutex_t *m, 
	char *file, int line)
{
	if(!current_task || panicing)
		return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "mutex_off got invalid mutex: %x (%x): %s:%d\n", 
				m, m->magic, file, line);
	if((unsigned)m->pid != current_task->pid)
		panic(0, "Process %d attempted to release mutex that it didn't own", 
				current_task->pid);
	assert(m->count > 0);
	engage_full_system_lock();
	if(!A_dec_mutex_count(m))
		reset_mutex(m);
	if(!m->count && m->pid != -1)
		panic(0, "Mutex failed to reset");
	disengage_full_system_lock();
	__super_sti();
}

__attribute__((optimize("O0"))) void __destroy_mutex(mutex_t *m, 
	char *file, int line)
{
	if(!m || panicing) return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "destroy_mutex got invalid mutex: %x: %s:%d\n", m, file, line);
	engage_full_system_lock();
	reset_mutex(m);
	m->magic = 0xDEADBEEF;
	if(m->flags & MF_ALLOC)
		kfree((void *)m);
	disengage_full_system_lock();
}

void init_mutexes()
{
	
}
