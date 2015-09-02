#ifndef RWLOCK_H
#define RWLOCK_H
#include <stdatomic.h>
typedef struct {
	unsigned magic, flags;
	_Atomic unsigned long readers;
	atomic_flag writer;
	char *holderfile;
	int holderline;
} rwlock_t;

enum rwlock_locktype {
	RWL_READER,
	RWL_WRITER
};

#define RWLOCK_MAGIC 0x1AD1E5

#define RWL_ALLOC  0x4

rwlock_t *rwlock_create(rwlock_t *lock);
void rwlock_destroy(rwlock_t *lock);
void __rwlock_acquire(rwlock_t *lock, enum rwlock_locktype, char *, int);
void rwlock_release(rwlock_t *lock, enum rwlock_locktype);
void __rwlock_deescalate(rwlock_t *lock, char *file, int line);

#define rwlock_acquire(a, b) __rwlock_acquire(a, b, __FILE__, __LINE__)
#define rwlock_escalate(a, b) __rwlock_escalate(a, b, __FILE__, __LINE__)

#endif
