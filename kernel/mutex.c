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
mutex_t *mutex_list=0;
#undef DEBUG
static inline int A_inc_mutex_count(mutex_t *m)
{
	asm("lock incl %0":"+m"(m->count));
	return m->count;
}

static inline int A_dec_mutex_count(mutex_t *m)
{
	asm("lock decl %0":"+m" (m->count));
	return m->count;
}

void reset_mutex(mutex_t *m)
{
	__super_cli();
	m->pid=-1;
	m->owner=m->count=0;
	m->magic = MUTEX_MAGIC;
	*m->file=0;
}

static void add_mutex_list(mutex_t *m)
{
	mutex_t *old = mutex_list;
	mutex_list = m;
	if(old) old->prev = m;
	m->next = old;
	m->prev=0;
}

static void remove_mutex_list(mutex_t *m)
{
	if(!m) return;
	if(m->prev)
		m->prev->next = m->next;
	if(m->next)
		m->next->prev = m->prev;
	if(mutex_list == m)
		mutex_list = m->next;
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
	super_cli();
	task_critical();
	reset_mutex(m);
	add_mutex_list(m);
	super_sti();
	task_uncritical();
	m->magic = MUTEX_MAGIC;
	return m;
}

/* Void because we must garuntee that this succeeds or panics */
__attribute__((optimize("O0"))) void __mutex_on(mutex_t *m, char *file, int line)
{
	if(!m) return;
	if(!current_task || panicing) return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "got invalid mutex: %x: %s:%d\n", m, file, line);
	int flag=0;
	volatile int i=0;
	task_critical();
	char found = (current_task->flags & TF_LOCK) ? 1 : 0;
	raise_flag(TF_LOCK);
	/* This must be garunteed to only break when the mutex is free. */
	while(m->count > 0 && (unsigned)m->pid != current_task->pid) {
#if 1
		assert(m->pid != -1);
		task_t *t = (task_t *)m->owner;
		task_t *p = 0;//get_task_pid(t->pid);
		if(p && (p != t || m->pid != (int)p->pid || m->pid != (int)t->pid))
			panic(0, "Mutex confusion (%d)! \nm %d, t %d (%d:%d:%d), p %d; %s:%d\nWas set on: %s:%d", current_task->pid, m->pid, t->pid, t->state, t->system, t->flags, p->pid, file, line, m->file, m->line);
#endif
		raise_flag(TF_WMUTEX);
		task_full_uncritical();
		lower_flag(TF_LOCK);
		/* Sleep...*/
		if(i++ == 1000)
			printk(0, "[mutex]: potential deadlock:\n\ttask %d is waiting for a mutex (p=%d,c=%d)\n", current_task->pid, m->pid, m->count);
		force_schedule();
		
		task_critical();
		raise_flag(TF_LOCK);
	}
	/* Here we update the mutex fields */
	raise_flag(TF_DIDLOCK);
	lower_flag(TF_WMUTEX);
#ifdef DEBUG
	strcpy((char *)m->file, file);
	m->line = line;
#endif
	if(((unsigned)m->pid != current_task->pid && m->count))
		panic(0, "Mutex lock failed %d %d\n", m->pid, m->count);
	m->pid = current_task->pid;
	m->owner = (unsigned)current_task;
	
	if(m->count >= MUTEX_COUNT)
		panic(0, "Mutex overran count at %s:%d\n", file, line);
	
	A_inc_mutex_count(m);
	/* Clear up locks and return */
	task_full_uncritical();
	if(!found)
		lower_flag(TF_LOCK);
	else
		raise_flag(TF_LOCK);
	__super_sti();
	assert((unsigned)m->pid == current_task->pid);
	assert(m->count > 0);
}

__attribute__((optimize("O0"))) void __mutex_off(mutex_t *m, char *file, int line)
{
	if(!current_task || panicing)
		return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "got invalid mutex: %x: %s:%d\n", m, file, line);
	if(m->pid != get_pid()) {
#ifdef DEBUG
		panic(0, "Tried to free a mutex that we don't own! at %s:%d\ngrabbed at %s:%d, owned by %d (c=%d) (we are %d)", file, line, m->file, m->line, m->pid, m->count, get_pid());
#else
		panic(0, "Process %d attempted to release mutex that it didn't own", current_task->pid);
#endif
	}
	assert(m->count > 0);
	task_critical();
	if(!A_dec_mutex_count(m)) {
		reset_mutex(m);
	}
	if(!m->count && m->pid != -1)
		panic(0, "Mutex failed to reset");
	task_full_uncritical();
	__super_sti();
}

void __destroy_mutex(mutex_t *m, char *file, int line)
{
	if(!m || panicing) return;
	if(m->magic != MUTEX_MAGIC)
		panic(0, "got invalid mutex: %x: %s:%d\n", m, file, line);
	task_critical();
	super_cli();
	task_critical();
	reset_mutex(m);
	remove_mutex_list(m);
	task_uncritical();
	super_sti();
	if(m->flags & MF_ALLOC)
		kfree((void *)m);
	task_uncritical();
}

/** THIS IS VERY SLOW **/
void force_nolock(task_t *t)
{
	return;
	if(panicing) return;
#ifndef DEBUG
	return;
#endif
	super_cli();
	mutex_t *m = mutex_list;
	task_critical();
	while(m)
	{
		if(m->pid == (int)t->pid)
			panic(0, "Found invalid mutex! %d %d, %s:%d", m->pid, m->count, m->file, m->line);
		m = m->next;
	}
	task_uncritical();
}

/** THIS IS VERY SLOW **/
void do_force_nolock(task_t *t)
{
	super_cli();
	mutex_t *m = mutex_list;
	task_critical();
	while(m)
	{
		if(m->pid == (int)t->pid)
			panic(0, "Found invalid mutex! %d %d, %s:%d", m->pid, m->count, m->file, m->line);
		m = m->next;
	}
	task_uncritical();
}

void unlock_all_mutexes()
{
	if(panicing)
		return;
	mutex_t *m = mutex_list;
	task_critical();
	while(m)
	{
		reset_mutex(m);
		m = m->next;
	}
	task_t *t = (task_t *)kernel_task;
	while(t)
	{
		t=t->next;
	}
	task_uncritical();
}

int proc_read_mutex(char *buf, int off, int len)
{
	int total_len=0;
	total_len += proc_append_buffer(buf, "PID | COUNT\n", total_len, -1, off, len);
	mutex_t *m = mutex_list;
	int g=0;
	while(m)
	{
		char tmp[64];
		memset(tmp, 0, 64);
		if(m->pid != -1 || m->count) {
			sprintf(tmp, "%4d | %5d: %s\n", m->pid, m->count, m->file);
			total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
			g++;
		}
		m = m->next;
	}
	return total_len;
}
