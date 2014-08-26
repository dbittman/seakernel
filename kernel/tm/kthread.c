#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/tm/schedule.h>
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
	/* TODO: This doesn't need to be a real process...it can be a thread. But, for now,
	 * this is simpler */
	int pid = tm_fork();
	if(!pid) {
		/* kernel threads have no parent (since we don't do a wait() for them), and
		 * they have root-like abilities. They are also constantly 'in the system',
		 * and so they have their syscall num set to -1. Also, no regs are set, since
		 * we can't do any weird iret calls or anything. */
		current_task->parent = 0;
		current_task->thread->real_uid = current_task->thread->effective_uid = 
			current_task->thread->real_gid = current_task->thread->effective_gid = 0;
		current_task->flags |= TF_KTASK;
		current_task->system = -1;
		current_task->regs = current_task->sysregs = 0;
		strncpy((char *)current_task->command, name, 128);
		
		/* free up the directory save for the stack and the kernel stuff, since we
		 * don't need it */
		mm_free_thread_shared_directory();
		/* okay, call the thread entry function */
		kt->code = entry(kt, arg);
		/* get the code FIRST, since once we set KT_EXITED, we can't rely on kt
		 * being a valid pointer */
		int code = kt->code;
		or_atomic(&kt->flags, KT_EXITED);
		if(current_task->flags & TF_FORK_COPIEDUSER) {
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
		panic(0, "kthread lived past exit");
	}
	kt->pid = pid;
	kt->process = tm_get_process_by_pid(pid);
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
	if(!(flags & KT_JOIN_NONBLOCK))
		kthread_wait(kt, 0);
	if(kt->flags & KT_EXITED)
		return kt->code;
	return -1;
}

void kthread_kill(struct kthread *kt, int flags)
{
	if(kt->pid && kt->pid == current_task->pid)
		panic(0, "kthread tried to commit suicide");
	if(!kt->pid)
		panic(0, "cannot kill unknown kthread");
	tm_kill_process(kt->pid);
}

