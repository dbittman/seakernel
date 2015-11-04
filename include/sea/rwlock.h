#ifndef RWLOCK_H
#define RWLOCK_H
#include <stdatomic.h>
struct rwlock {
	unsigned magic, flags;
	_Atomic unsigned long readers;
	atomic_flag writer;
	char *holderfile;
	int holderline;
};

enum rwlock_locktype {
	RWL_READER,
	RWL_WRITER
};

#define RWLOCK_MAGIC 0x1AD1E5

#define RWL_ALLOC  0x4

struct rwlock *rwlock_create(struct rwlock *lock);
void rwlock_destroy(struct rwlock *lock);
void __rwlock_acquire(struct rwlock *lock, enum rwlock_locktype, char *, int);
void rwlock_release(struct rwlock *lock, enum rwlock_locktype);
void __rwlock_deescalate(struct rwlock *lock, char *file, int line);

#define rwlock_acquire(a, b) __rwlock_acquire(a, b, __FILE__, __LINE__)
#define rwlock_escalate(a, b) __rwlock_escalate(a, b, __FILE__, __LINE__)

#endif
