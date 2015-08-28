#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/errno.h>

static void get_status_int(struct process *t, int *st, int *__pid)
{
	int ret_val, sig_number;
	int status = t->exit_reason.cause;
	
	sig_number = t->exit_reason.sig;
	ret_val = t->exit_reason.ret;
	status |= t->exit_reason.cause | t->exit_reason.coredump;
	if(__pid) *__pid = t->pid;
	/* yeah, lots of weird posix stuff going on here */
	short code=0;
	short info=0;
	if(status == __EXITSIG)  code = 0x7e, info=(char)sig_number << 8;
	if(status == __STOPSIG) {
		if(status & __EXITSIG) panic(PANIC_NOSYNC, 
			"Stat of dead task returned nonsense data!");
		code |= 0x7f, info=(char)sig_number << 8;
	}
	if(status == __EXIT) {
		info=ret_val<<8;
	}
	if(status == __STOPPED)
		info=0, code = 0x7f;
	if(st)
		*st = code | info;
}

static struct process *__find_first_child(struct process *parent)
{
	struct llistnode *node;
	struct process *proc;
	rwlock_acquire(&process_list->rwl, RWL_READER);
	ll_for_each_entry(process_list, node, struct process *, proc) {
		if(proc->parent == parent) {
			tm_process_inc_reference(proc);
			rwlock_release(&process_list->rwl, RWL_READER);
			return proc;
		}
	}
	rwlock_release(&process_list->rwl, RWL_READER);
	return 0;
}

/* So, if a process enters the while loop below, but before it
 * blocks another process actually sets it's state to stopped or
 * whatever and then wakes the blocklist before the waiting process
 * can block, we'll lock up. Scheduling a final check for after the
 * process blocks solves this problem. */
static void __wait_check_reset(unsigned long arg)
{
	struct process *proc = (void *)arg;
	if((proc->flags & PROCESS_EXITED)
			|| proc->exit_reason.cause) {
		tm_blocklist_wakeall(&proc->waitlist);
	}
}

int sys_waitpid(int pid, int *st, int opt)
{
	if(current_process->pid)
	if(!pid || pid < -1)
		return -ENOSYS;

	struct process *proc = 0;
	if(pid == -1) {
		proc = __find_first_child(current_process);
	} else {
		proc = tm_process_get(pid);
	}

	if(!proc || proc->parent != current_process) {
		if(proc)
			tm_process_put(proc);
		return -ECHILD;
	}

	while(!(proc->flags & PROCESS_EXITED)
			&& (proc->exit_reason.cause != __STOPSIG)
			&& (proc->exit_reason.cause != __STOPPED)
			&& !(opt & WNOHANG)) {
		/* do this inside the loop, as it's rare for this loop body to execute more than once */
		async_call_create(&current_thread->waitcheck_call, 0,
				__wait_check_reset, (unsigned long)proc, ASYNC_CALL_PRIORITY_LOW);
		int r = tm_thread_block_schedule_work(&proc->waitlist, THREADSTATE_INTERRUPTIBLE,
				&current_thread->waitcheck_call); 
		struct workqueue *wq = current_thread->waitcheck_call.queue;
		if(wq)
			workqueue_delete(wq, &current_thread->waitcheck_call);
		switch(r) {
			case -ERESTART:
				tm_process_put(proc);
				return -ERESTART;
			case -EINTR:
				tm_process_put(proc);
				return -EINTR;
		}
	}

	if(proc->exit_reason.cause == __STOPPED || proc->exit_reason.cause == __STOPSIG) {
		int code, gotpid;
		get_status_int(proc, &code, &gotpid);
		tm_process_put(proc);
		if(st)
			*st = code;
		return gotpid;
	}

	if(!(proc->flags & PROCESS_EXITED)) {
		tm_process_put(proc);
		return 0;
	}
	int code, gotpid;
	get_status_int(proc, &code, &gotpid);
	if(pid == -1)
		tm_process_wait_cleanup(proc);
	tm_process_put(proc);
	if(st)
		*st = code;
	return gotpid;
}

int sys_wait3(int *a, int b, int *c)
{
	return sys_waitpid(-1, a, b);
}

