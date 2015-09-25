#ifndef __SEA_SPINLOCK_H
#define __SEA_SPINLOCK_H

#include <stdatomic.h>
#include <sea/string.h>
struct spinlock {
	atomic_flag flag;
};

#include <sea/cpu/processor.h>
static inline struct spinlock *spinlock_create(struct spinlock *s)
{
	assertmsg(s, "allocating spinlocks is not allowed");
	memset(s, 0, sizeof(*s));
	return s;
}

static inline void spinlock_acquire(struct spinlock *s)
{
	cpu_disable_preemption();
	while(atomic_flag_test_and_set_explicit(&s->flag, memory_order_relaxed)) {
		asm("pause"); //TODO
	}
}

static inline void spinlock_release(struct spinlock *s)
{
	atomic_flag_clear_explicit(&s->flag, memory_order_relaxed);
	cpu_enable_preemption();
}

static inline void spinlock_destroy(struct spinlock *s)
{
	/* well, this function is basically useless. */
}

#endif

