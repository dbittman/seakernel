/* rwlock.c - copyright Daniel Bittman 2013
 * implements rwlock procedures
 *
 * each rwlock may have any number of readers, but only one writer. Also
 * a writer may only clench a lock if there are zero readers, and if
 * a writer has the lock, no readers may lock it. If a lock cannot be
 * acquired, it will go into sleep until the lock can be acquired
 */
#include <sea/kernel.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/tm/process.h>
#include <sea/tm/schedule.h>
#undef DEBUG
#define DEBUG 0
void __rwlock_acquire(rwlock_t *lock, unsigned flags, char *file, int line)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
#ifdef RWL_DEBUG
	  printk(0, "TRACE: %d: acquire rwl %x (%d) (%d): %s:%d\n", current_task->pid, lock, lock->locks, flags, file, line);
#endif
	while(1) 
	{
		/* if we're trying to get a writer lock, we need to wait until the
		* lock is completely cleared */
#if DEBUG
		int timeout = 100000;
		while((flags & RWL_WRITER) && lock->locks && --timeout) schedule();
		if(timeout == 0)
			panic(0, "(1) waited too long to acquire the lock:%s:%d\n", file, line);
		/* now, spinlock-acquire the write_lock bit */
		timeout = 100000;
#else
		while((flags & RWL_WRITER) && lock->locks) tm_schedule();
#endif
		/* now try to get the write lock so we have exclusive access
		 * to the lock itself */
#if DEBUG
		while(bts_atomic(&lock->locks, 0) && --timeout)
			schedule();
		if(timeout == 0)
			panic(0, "(2) waited too long to acquire the lock:%s:%d\n", file, line);
#else
		while(bts_atomic(&lock->locks, 0))
			tm_schedule();
#endif
		/* if we're trying to read, we need to increment the locks by 2
		 * thus skipping over the write_lock bit */
		if(flags & RWL_READER) {
			add_atomic(&lock->locks, 2);
			/* and now reset the write_lock */
			btr_atomic(&lock->locks, 0);
			break;
		}
		if(flags & RWL_WRITER) {
			/* if we were able to get the lock (write_lock bit is set, 
			 * and there are no readers), then we break. Otherwise, we 
			 * reset the write_bit and try again */
			if(lock->locks == 1)
				break;
			else
				btr_atomic(&lock->locks, 0);
		}
	}
}

void __rwlock_escalate(rwlock_t *lock, unsigned flags, char *file, int line)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	assert(lock->locks);
#ifdef RWL_DEBUG
	printk(0, "TRACE: %d: escalate rwl %x (%d) (%d): %s:%d\n", current_task->pid, lock, lock->locks, flags, file, line);
#endif
	if(lock->locks == 1 && (flags & RWL_READER)) {
		/* change from a writer lock to a reader lock. This is easy. */
		add_atomic(&lock->locks, 2);
		/* and now reset the write_lock */
		btr_atomic(&lock->locks, 0);
	} else if(lock->locks % 2 == 0 && (flags & RWL_WRITER)) {
		/* change from a reader to a writer. This is, of course,
		 * less simple. We must wait until we are the only reader, and
		 * then attempt a switch */
		while(1) {
#if DEBUG
			int timeout = 100000;
			while(lock->locks != 2 && --timeout) schedule();
			if(timeout == 0)
				panic(0, "(1) waited too long to acquire the lock:%s:%d\n", file, line);
			timeout=100000;
#else
			while(lock->locks != 2) tm_schedule();
#endif
			/* now, spinlock-acquire the write_lock bit */
#if DEBUG
			while(bts_atomic(&lock->locks, 0) && --timeout)
				schedule();
			if(timeout == 0)
				panic(0, "(2) waited too long to acquire the lock:%s:%d\n", file, line);
#else
			while(bts_atomic(&lock->locks, 0))
				tm_schedule();
#endif
			if(lock->locks == 3)
			{
				/* remove our read lock */
				sub_atomic(&lock->locks, 2);
				break;
			}
			/* failed to grab the write_lock bit before a task
			 * added itself */
			btr_atomic(&lock->locks, 0);
		}
	}
}

void rwlock_release(rwlock_t *lock, unsigned flags)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	assert(lock->locks);
#ifdef RWL_DEBUG
	printk(0, "TRACE: release rwl (%d) (%d)\n", lock->locks, flags);
#endif
	if(flags & RWL_READER) {
		assert(lock->locks >= 2);
		sub_atomic(&lock->locks, 2);
	}
	else if(flags & RWL_WRITER) {
		assert(lock->locks == 1);
		btr_atomic(&lock->locks, 0);
	}
}

rwlock_t *rwlock_create(rwlock_t *lock)
{
	if(!lock) {
		lock = (void *)kmalloc(sizeof(rwlock_t));
		lock->flags = RWL_ALLOC;
	} else
		memset((void *)lock, 0, sizeof(rwlock_t));
	lock->locks = 0;
	lock->magic = RWLOCK_MAGIC;
	return lock;
}

void rwlock_destroy(rwlock_t *lock)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	lock->magic=0;
	if(lock->flags & RWL_ALLOC) 
		kfree((void *)lock);
}
