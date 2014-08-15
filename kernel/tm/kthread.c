#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/tm/schedule.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <sea/cpu/atomic.h>
#include <sea/string.h>
#include <sea/boot/init.h>

struct kthread *tm_kthread_create(struct kthread *kt, const char *name, int flags,
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
	int pid = tm_fork();
	if(!pid) {
		current_task->parent = 0;
		current_task->thread->real_uid = current_task->thread->effective_uid = 
			current_task->thread->real_gid = current_task->thread->effective_gid = 0;
		current_task->flags |= TF_KTASK;
		current_task->system = -1;
		current_task->regs = current_task->sysregs = 0;
		strncpy((char *)current_task->command, name, 128);
		
		mm_free_thread_shared_directory();
		kt->code = entry(kt, arg);
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
	}
	kt->pid = pid;
	return kt;
}

void tm_kthread_destroy(struct kthread *kt)
{
	if(!(kt->flags & KT_EXITED))
		panic(0, "tried to destroy a running kernel thread");
	if(kt->flags & KT_ALLOC)
		kfree(kt);
}

int tm_kthread_wait(struct kthread *kt, int flags)
{
	or_atomic(&kt->flags, KT_WAITING);
	while(!(kt->flags & KT_EXITED)) {
		if(flags & KT_WAIT_NONBLOCK)
			return -1;
		tm_schedule();
	}
	return kt->code;
}

int tm_kthread_join(struct kthread *kt, int flags)
{
	or_atomic(&kt->flags, KT_JOIN);
	if(!(flags & KT_JOIN_NONBLOCK))
		tm_kthread_wait(kt, 0);
	if(kt->flags & KT_EXITED)
		return kt->code;
	return -1;
}

void tm_kthread_kill(struct kthread *kt, int flags)
{
	if(kt->pid && kt->pid == current_task->pid)
		panic(0, "kthread tried to commit suicide");
	if(!kt->pid)
		panic(0, "cannot kill unknown kthread");
	tm_kill_process(kt->pid);
}

