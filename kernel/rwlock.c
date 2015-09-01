/* rwlock.c - copyright Daniel Bittman 2013
 * implements rwlock procedures
 *
 * each rwlock may have any number of readers, but only one writer. Also
 * a writer may only clench a lock if there are zero readers, and if
 * a writer has the lock, no readers may lock it. If a lock cannot be
 * acquired, it will go into sleep until the lock can be acquired
 */
#include <sea/kernel.h>
#include <stdatomic.h>
#include <sea/rwlock.h>
#include <sea/tm/process.h>
#include <sea/mm/kmalloc.h>
void __rwlock_acquire(rwlock_t *lock, enum rwlock_locktype type, char *file, int line)
{
	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(current_thread && current_thread->interrupt_level)
		panic(PANIC_NOSYNC, "cannot lock an rwlock within interrupt context");
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	while(atomic_flag_test_and_set_explicit(&lock->writer, memory_order_acquire)) {
		tm_schedule();
	}

	if(type == RWL_READER) {
		atomic_fetch_add(&lock->readers, 1);
		atomic_flag_clear_explicit(&lock->writer, memory_order_release);
	} else {
		while(lock->readers != 0)
			tm_schedule();
	}
}

void __rwlock_deescalate(rwlock_t *lock, char *file, int line)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	
	atomic_fetch_add_explicit(&lock->readers, 1, memory_order_acquire);
	atomic_flag_clear_explicit(&lock->writer, memory_order_release);
}

void rwlock_release(rwlock_t *lock, enum rwlock_locktype type)
{
	assert(lock->magic == RWLOCK_MAGIC);
	if(kernel_state_flags & KSF_DEBUGGING)
		return;
	if(kernel_state_flags & KSF_SHUTDOWN) return;
	if(type == RWL_READER) {
		assert(lock->readers >= 1);
		atomic_fetch_sub_explicit(&lock->readers, 1, memory_order_release);
	} else {
		assert(lock->readers == 0);
		atomic_flag_clear_explicit(&lock->writer, memory_order_release);
	}
}

rwlock_t *rwlock_create(rwlock_t *lock)
{
	if(!lock) {
		lock = (void *)kmalloc(sizeof(rwlock_t));
		lock->flags = RWL_ALLOC;
	} else {
		memset((void *)lock, 0, sizeof(rwlock_t));
	}
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
