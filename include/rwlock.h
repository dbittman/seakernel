#ifndef RWLOCK_H
#define RWLOCK_H

typedef volatile struct {
	volatile unsigned magic, flags;
	volatile unsigned long locks;
} rwlock_t;

#define RWLOCK_MAGIC 0x1AD1E5

#define RWL_READER 0x1
#define RWL_WRITER 0x2
#define RWL_ALLOC  0x4

rwlock_t *rwlock_create(rwlock_t *lock);
void rwlock_destroy(rwlock_t *lock);
void rwlock_acquire(rwlock_t *lock, unsigned flags);
void rwlock_release(rwlock_t *lock, unsigned flags);

#endif
