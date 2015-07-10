#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <sea/cpu/atomic.h>
#include <sea/string.h>
#include <sea/boot/init.h>

struct kthread *kthread_create(struct kthread *kt, const char *name, int flags,
		int (*entry)(struct kthread *, void *), void *arg)
{
	if(!kt) {
		kt = kmalloc(sizeof(struct kthread));
		kt->flags = KT_ALLOC;
	} else {
		kt->flags = 0;
	}

	kt->flags |= flags;
	kt->entry = entry;
	kt->arg = arg;
	/* TODO: this could be a thread, but we'd need to clone from the kernel directory ... */
	int tid = sys_clone(CLONE_SHARE_PROCESS);
	if(!tid) {
		/* kernel threads have no parent (since we don't do a wait() for them), and
		 * they have root-like abilities. They are also constantly 'in the system',
		 * and so they have their syscall num set to -1. Also, no regs are set, since
		 * we can't do any weird iret calls or anything. */
		current_process->parent = 0;
		current_process->real_uid = current_process->effective_uid = 
			current_process->real_gid = current_process->effective_gid = 0;
		current_thread->flags |= TF_KTASK;
		current_thread->system = -1;
		current_thread->regs = 0;
		strncpy((char *)current_process->command, name, 128);
		
		/* free up the directory save for the stack and the kernel stuff, since we
		 * don't need it TODO */
		//mm_free_thread_shared_directory();
		/* okay, call the thread entry function */
		kt->code = entry(kt, arg);
		/* get the code FIRST, since once we set KT_EXITED, we can't rely on kt
		 * being a valid pointer */
		int code = kt->code;
		or_atomic(&kt->flags, KT_EXITED);
		/* TODO: figure this out */
#if 0
		if(current_thread->flags & TF_FORK_COPIEDUSER) {
			/* HACK: this will cause the stack to switch over to the kernel stack
			 * when exit is called, allowing us to free the whole page directory.
			 * There is probably a better way to do this, but it would require a
			 * switch_stack_and_call_function function, which I don't feel like
			 * writing.
			 */
			tm_switch_to_user_mode();
			u_exit(code);
		} else {
			tm_exit(0);
		}
#endif
		panic(0, "kthread lived past exit");
	}
	kt->tid = tid;
	/* TODO */
	//kt->thread = tm_get_process_by_pid(pid);
	return kt;
}

void kthread_destroy(struct kthread *kt)
{
	if(!(kt->flags & KT_EXITED))
		panic(0, "tried to destroy a running kernel thread");
	if(kt->flags & KT_ALLOC)
		kfree(kt);
}

int kthread_wait(struct kthread *kt, int flags)
{
	/* eh, the thread may wish to know this */
	or_atomic(&kt->flags, KT_WAITING);
	while(!(kt->flags & KT_EXITED)) {
		/* if we don't want to block, and the thread hasn't exited,
		 * then return -1 */
		if(flags & KT_WAIT_NONBLOCK) {
			and_atomic(&kt->flags, ~KT_WAITING);
			return -1;
		}
		tm_schedule();
	}
	/* the thread has exited. return the exit code */
	and_atomic(&kt->flags, ~KT_WAITING);
	return kt->code;
}

int kthread_join(struct kthread *kt, int flags)
{
	or_atomic(&kt->flags, KT_JOIN);
	tm_thread_resume(kt->thread); /* in case it's sleeping */
	if(!(flags & KT_JOIN_NONBLOCK))
		kthread_wait(kt, 0);
	if(kt->flags & KT_EXITED)
		return kt->code;
	return -1;
}

void kthread_kill(struct kthread *kt, int flags)
{
	if(kt->tid && kt->tid == current_thread->tid)
		panic(0, "kthread tried to commit suicide");
	if(!kt->tid)
		panic(0, "cannot kill unknown kthread");
	tm_thread_kill(kt->thread);
}

