#include <sea/tm/thread.h>
#include <sea/cpu/processor.h>
#include <sea/spinlock.h>
struct spinlock *spinlock_create(struct spinlock *s)
{
	assertmsg(s, "allocating spinlocks is not allowed");
	memset(s, 0, sizeof(*s));
	return s;
}

void spinlock_acquire(struct spinlock *s)
{
	cpu_disable_preemption();
	while(atomic_flag_test_and_set_explicit(&s->flag, memory_order_relaxed)) {
		arch_cpu_pause();
	}
}

void spinlock_release(struct spinlock *s)
{
	atomic_flag_clear_explicit(&s->flag, memory_order_relaxed);
	cpu_enable_preemption();
}

void spinlock_destroy(struct spinlock *s)
{
	/* well, this function is basically useless. */
}


