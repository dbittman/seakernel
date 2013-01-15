/* rwlock.c - copyright Daniel Bittman 2013
 * implements rwlock procedures
 *
 * each rwlock may have any number of readers, but only one writer. Also
 * a writer may only clench a lock if there are zero readers, and if
 * a writer has the lock, no readers may lock it. If a lock cannot be
 * acquired, it will go into sleep until the lock can be acquired
 */
#include <kernel.h>
#include <atomic.h>
#include <rwlock.h>
#include <task.h>

void rwlock_acquire(rwlock_t *lock, unsigned flags)
{
	assert(lock->magic == RWLOCK_MAGIC);
	while(1) 
	{
		/* if we're trying to get a writer lock, we need to wait until the
		 * lock is completely cleared */
		while((flags & RWL_WRITER) && lock->locks) schedule();
		/* now, spinlock-acquire the write_lock bit */
		while(bts_atomic(&lock->locks, 0))
			schedule();
		/* if we're trying to read, we need to increment the locks by 2
		 * thus skipping over the write_lock bit */
		if(flags & RWL_READER) {
			add_atomic(&lock->locks, 2);
			/* and now reset the write_lock */
			btr_atomic(&lock->locks, 0);
			assert(lock->locks && !(lock->locks & 1));
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

void rwlock_release(rwlock_t *lock, unsigned flags)
{
	assert(lock->magic == RWLOCK_MAGIC);
	assert(lock->locks);
	if(flags & RWL_READER)
		sub_atomic(&lock->locks, 2);
	else if(flags & RWL_WRITER)
		btr_atomic(&lock->locks, 0);
}

rwlock_t *rwlock_create(rwlock_t *lock)
{
	if(!lock) {
		lock = (void *)kmalloc(sizeof(rwlock_t));
		lock->flags = RWL_ALLOC;
	} else
		lock->flags = 0;
	lock->locks = 0;
	lock->magic = RWLOCK_MAGIC;
	return lock;
}

void rwlock_destroy(rwlock_t *lock)
{
	assert(lock->magic == RWLOCK_MAGIC);
	lock->magic=0;
	assert(!lock->locks);
	if(lock->flags & RWL_ALLOC) 
		kfree((void *)lock);
}
