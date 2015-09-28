#ifndef __SEA_SPINLOCK_H
#define __SEA_SPINLOCK_H

#include <stdatomic.h>
#include <sea/string.h>
struct spinlock {
	atomic_flag flag;
};

void spinlock_destroy(struct spinlock *s);
void spinlock_release(struct spinlock *s);
void spinlock_acquire(struct spinlock *s);
struct spinlock *spinlock_create(struct spinlock *s);
#endif

