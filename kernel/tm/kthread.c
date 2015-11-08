#include <sea/kernel.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <stdatomic.h>
#include <sea/string.h>
#include <sea/boot/init.h>
#include <sea/kobj.h>

static void __do_kthread_entry(void)
{
	struct kthread *kt = current_thread->kernel_thread;
	tm_thread_raise_flag(current_thread, THREAD_KERNEL);
	current_thread->regs = 0;
	kthread_exit(kt, kt->entry(kt, kt->arg));
}

struct kthread *kthread_create(struct kthread *kt, const char *name, int flags,
		int (*entry)(struct kthread *, void *), void *arg)
{
	KOBJ_CREATE(kt, flags, KT_ALLOC);
	kt->entry = entry;
	kt->arg = arg;
	tm_clone(CLONE_SHARE_PROCESS | CLONE_KTHREAD, __do_kthread_entry, kt);
	return kt;
}

void kthread_destroy(struct kthread *kt)
{
	if(!(kt->flags & KT_EXITED))
		panic(0, "tried to destroy a running kernel thread");
	tm_thread_put(kt->thread);
	KOBJ_DESTROY(kt, KT_ALLOC);
}

int kthread_wait(struct kthread *kt, int flags)
{
	/* eh, the thread may wish to know this */
	atomic_fetch_or_explicit(&kt->flags, KT_WAITING, memory_order_acquire);
	while(!(kt->flags & KT_EXITED)) {
		/* if we don't want to block, and the thread hasn't exited,
		 * then return -1 */
		if(flags & KT_WAIT_NONBLOCK) {
			atomic_fetch_and_explicit(&kt->flags, ~KT_WAITING, memory_order_release);
			return -1;
		}
		tm_schedule();
	}
	/* the thread has exited. return the exit code */
	atomic_fetch_and_explicit(&kt->flags, ~KT_WAITING, memory_order_release);
	return kt->code;
}

int kthread_join(struct kthread *kt, int flags)
{
	atomic_fetch_or(&kt->flags, KT_JOIN);
	tm_thread_poke(kt->thread); /* in case it's sleeping */
	if(!(flags & KT_JOIN_NONBLOCK))
		kthread_wait(kt, 0);
	if(kt->flags & KT_EXITED)
		return kt->code;
	return -1;
}

void kthread_kill(struct kthread *kt, int flags)
{
	if(kt->thread == current_thread)
		panic(0, "kthread tried to commit suicide");
	if(!kt->thread)
		panic(0, "cannot kill unknown kthread");
	tm_signal_send_thread(kt->thread, SIGKILL);
}

