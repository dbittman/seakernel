#ifndef __SEA_TM_KTHREAD_H
#define __SEA_TM_KTHREAD_H

#include <sea/tm/process.h>

struct kthread {
	int flags, code;
	unsigned pid;
	int (*entry)(struct kthread *, void *);
	void *arg;
	struct thread *thr;
};

#define KT_ALLOC   1
#define KT_JOIN    2
#define KT_EXITED  4
#define KT_WAITING 8

#define KT_WAIT_NONBLOCK 1
#define KT_JOIN_NONBLOCK 1

struct kthread *kthread_create(struct kthread *kt, const char *name, int flags, int (*entry)(struct kthread *, void *), void *arg);
void kthread_destroy(struct kthread *kt);
int kthread_wait(struct kthread *kt, int flags);
int kthread_join(struct kthread *kt, int flags);
void kthread_kill(struct kthread *kt, int flags);

#define kthread_is_joining(kt) (kt->flags & KT_JOIN)
#define kthread_is_waiting(kt) (kt->flags & KT_WAITING)

#endif

