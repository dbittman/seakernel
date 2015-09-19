#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/errno.h>
#include <sea/tm/timing.h>
#include <sea/cpu/interrupt.h>
#include <sea/mm/map.h>
int sys_sbrk(long inc)
{
	addr_t end = current_process->heap_end;
	if(inc > 0) {
		inc = ((inc - 1) & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
		current_process->heap_end += inc;
#warning "TODO: this causes a lot of entries in the mappings list. Merging!"
		addr_t ret = mm_mmap(end, inc,
				PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	}
	return end;
}

static void __alarm_timeout(unsigned long data)
{
	struct thread *thr = (struct thread *)data;
	if(thr->alarm_ticker) {
		tm_signal_send_thread(thr, SIGALRM);
	}
	thr->alarm_ticker = 0;
}

/* TODO: make this all thread safe with atomics */
int sys_alarm(int dur)
{
	struct async_call *call = async_call_create(&current_thread->alarm_timeout,
			0, &__alarm_timeout, (unsigned long)current_thread, ASYNC_CALL_PRIORITY_LOW);
	struct cpu *cpu = cpu_get_current();
	struct ticker *ticker = &cpu->ticker;
	cpu_put_current(cpu);

	if(dur == 0) {
		int old = cpu_interrupt_set(0);
		ticker = current_thread->alarm_ticker;
		if(ticker)
			ticker_delete(ticker, call);
		current_thread->alarm_ticker = 0;
		cpu_interrupt_set(old);
	} else {
		if(!current_thread->alarm_ticker) {
			current_thread->alarm_ticker = ticker;
			ticker_insert(ticker, dur * ONE_SECOND, call);
		}
	}
	return 0;
}

int sys_gsetpriority(int set, int which, int id, int val)
{
	if(set)
		return -ENOSYS;
	return current_thread->priority;
}

int sys_thread_setpriority(pid_t tid, int val, int flags)
{
	struct thread *thr = tm_thread_get(tid);
	if(!thr)
		return -ESRCH;
	if(thr->flags & THREAD_KERNEL) {
		tm_thread_put(thr);
		return -EPERM;
	}
	if(thr->process != current_process && current_process->effective_uid) {
		tm_thread_put(thr);
		return -EPERM;
	}
	if(!flags)
		thr->priority += -val;
	else
		thr->priority = (-val) + 1; /* POSIX has default 0, we use 1 */
	tm_thread_put(thr);
	return 0;
}

int sys_nice(int which, pid_t who, int val, int flags)
{
	if(which != PRIO_PROCESS)
		return -ENOTSUP;

	if(who != current_process->pid && current_process->effective_uid)
		return -EPERM;

	struct process *proc = tm_process_get(who);
	rwlock_acquire(&proc->threadlist.rwl, RWL_READER);
	struct thread *thr;
	struct llistnode *node;
	int ret = 0;
	ll_for_each_entry(&proc->threadlist, node, struct thread *, thr) {
		ret = sys_thread_setpriority(thr->tid, val, flags);
		if(ret < 0)
			break;
	}
	rwlock_release(&proc->threadlist.rwl, RWL_READER);
	tm_process_put(proc);
	return ret;
}

int sys_setsid(int ex, int cmd)
{
	if(cmd) {
		return -ENOTSUP;
	}
	current_process->tty=0;
	return 0;
}

int sys_setpgid(int a, int b)
{
	return -ENOSYS;
}

pid_t sys_get_pid(void)
{
	return current_process->pid;
}

pid_t sys_getppid(void)
{
	return current_process->parent->pid;
}

int tm_set_gid(int n)
{
	if(current_process->real_gid && current_process->effective_gid) {
		if(n == current_process->real_gid || n == current_process->saved_gid)
			current_process->effective_gid = n;
		else
			return -EPERM;
	} else {
		current_process->effective_gid = current_process->real_gid = current_process->saved_gid = n;
	}
	return 0;
}

int tm_set_uid(int n)
{
	if(current_process->real_uid && current_process->effective_uid) {
		if(n == current_process->real_uid || n == current_process->saved_uid)
			current_process->effective_uid = n;
		else
			return -EPERM;
	} else {
		current_process->effective_uid = current_process->real_uid = current_process->saved_uid = n;
	}
	return 0;
}

int tm_set_euid(int n)
{
	if(!current_process->real_uid 
		|| !current_process->effective_uid
		|| (n == current_process->real_uid)
		|| (n == current_process->saved_uid)) {
		current_process->effective_uid = n;
	} else
		return -EPERM;
	return 0;
}

int tm_set_egid(int n)
{
	if(!current_process->real_gid 
		|| !current_process->effective_gid
		|| (n == current_process->real_gid)
		|| (n == current_process->saved_gid)) {
		current_process->effective_gid = n;
	} else
		return -EPERM;
	return 0;
}

int tm_get_gid(void)
{
	return current_process->real_gid;
}

int tm_get_uid(void)
{
	return current_process->real_uid;
}

int tm_get_egid(void)
{
	return current_process->effective_gid;
}

int tm_get_euid(void)
{
	return current_process->effective_uid;
}


