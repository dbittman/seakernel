/* Functions for signaling threads and processes
 * signal.c: Copyright (c) 2015 Daniel Bittman
 */
#include <sea/boot/init.h>
#include <stdatomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/errno.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/vsprintf.h>
#include <sea/tm/blocking.h>

void tm_thread_handle_signal(int signal)
{
	struct sigaction *sa = &current_process->signal_act[signal];
	if(signal != SIGKILL && signal != SIGSTOP) {
		if((current_thread->sig_mask & (1 << signal))) {
			current_thread->signal = 0;
			return;
		}
	}
	if(signal != SIGKILL && signal != SIGSTOP && (addr_t)sa->_sa_func._sa_handler != SIG_IGN && 
			(addr_t)sa->_sa_func._sa_handler != SIG_DFL) {
		if(!(sa->sa_flags & SA_NODEFER))
			current_thread->sig_mask |= (1 << signal);
		tm_thread_raise_flag(current_thread, THREAD_SIGNALED);
	} else if(!current_thread->system && !(current_thread->flags & THREAD_KERNEL)
			&& ((addr_t)sa->_sa_func._sa_handler != SIG_IGN || signal == SIGKILL || signal == SIGSTOP)) {
		/* Default Handlers */
		tm_thread_raise_flag(current_thread, THREAD_SCHEDULE);
		current_thread->signal = 0;
		switch(signal)
		{
			case SIGHUP : case SIGKILL: case SIGQUIT: case SIGPIPE:
			case SIGBUS : case SIGABRT: case SIGTRAP: case SIGSEGV:
			case SIGALRM: case SIGFPE : case SIGILL : case SIGPAGE:
			case SIGINT : case SIGTERM: case SIGUSR1: case SIGUSR2:
				current_process->exit_reason.cause=__EXITSIG;
				current_process->exit_reason.sig=signal;
				tm_thread_exit(-9);
				break;
			case SIGUSLEEP:
				current_thread->state = THREADSTATE_UNINTERRUPTIBLE;
				break;
			case SIGSTOP: 
				if(!(sa->sa_flags & SA_NOCLDSTOP) && current_process->parent)
					tm_signal_send_process(current_process->parent, SIGCHILD);
				spinlock_acquire(&current_thread->status_lock);
				current_process->exit_reason.cause=__STOPSIG;
				current_process->exit_reason.sig=signal;
				current_thread->state = THREADSTATE_STOPPED;
				spinlock_release(&current_thread->status_lock);
				break;
			case SIGISLEEP:
				current_thread->state = THREADSTATE_INTERRUPTIBLE;
				break;
		}
	} else if(!current_thread->system && ((current_thread->flags & THREAD_KERNEL) || (addr_t)sa->_sa_func._sa_handler == SIG_IGN)) {
		current_thread->signal = 0;
	}
}

static int __can_send_signal(struct process *from, struct process *to, int signal)
{
	if(signal == SIGKILL && from->effective_uid)
		return 0;
	if(from->real_uid && from->effective_uid)
	{
		if(from->real_uid == to->real_uid
			|| from->real_uid == to->saved_uid
			|| from->effective_uid == to->real_uid
			|| from->effective_uid == to->saved_uid)
			return 1;
		else
			return 0;
	} else {
		return 1;
	}
}

void tm_signal_send_thread(struct thread *thr, int signal)
{
	assert(signal < NUM_SIGNALS);
	if(!(thr->flags & THREAD_PTRACED) || signal == SIGKILL) {
		atomic_fetch_or(&thr->signals_pending, 1 << (signal - 1));
	}
	tm_thread_raise_flag(thr, THREAD_SCHEDULE);
	if(thr->state == THREADSTATE_STOPPED && (signal == SIGCONT || signal == SIGKILL)) {
		spinlock_acquire(&thr->status_lock);
		thr->state = THREADSTATE_RUNNING;
		thr->process->exit_reason.cause = 0;
		thr->process->exit_reason.sig = 0;
		spinlock_release(&thr->status_lock);
	} else if((thr->flags & THREAD_PTRACED) && signal != SIGKILL) {
		spinlock_acquire(&thr->status_lock);
		thr->process->exit_reason.cause = __STOPSIG;
		thr->process->exit_reason.sig = signal;
		tm_blocklist_wakeall(&thr->process->waitlist);
		thr->state = THREADSTATE_STOPPED;
		spinlock_release(&thr->status_lock);
	}
	if(thr->state == THREADSTATE_INTERRUPTIBLE) {
		tm_thread_unblock(thr);
	}
}

int tm_signal_send_process(struct process *proc, int signal)
{
	assert(signal < NUM_SIGNALS);
	rwlock_acquire(&proc->threadlist.rwl, RWL_READER);
	struct llistnode *node;
	struct thread *thr;
	/* find the first thread that is willing to handle the signal */
	ll_for_each_entry(&proc->threadlist, node, struct thread *, thr) {
		if(!(thr->sig_mask & (1 << signal)) && signal != SIGKILL && signal != SIGSTOP) {
			rwlock_release(&proc->threadlist.rwl, RWL_READER);
			tm_signal_send_thread(thr, signal);
			return 0;
		}
	}
	rwlock_release(&proc->threadlist.rwl, RWL_READER);
	return -EACCES;
}

int sys_kill(pid_t pid, int signal)
{
	if(pid == 0 || signal < 0 || signal >= NUM_SIGNALS)
		return -EINVAL;
	struct process *proc = tm_process_get(pid);
	if(!proc)
		return -ESRCH;
	if(!__can_send_signal(current_process, proc, signal)) {
		tm_process_put(proc);
		return -EPERM;
	}
	tm_signal_send_process(proc, signal);
	tm_process_put(proc);
	return 0;
}

int sys_kill_thread(pid_t tid, int signal)
{
	if(tid == 0 || signal < 0 || signal >= NUM_SIGNALS)
		return -EINVAL;
	struct thread *thr = tm_thread_get(tid);
	if(!thr)
		return -ESRCH;
	if(thr->process != current_process) {
		tm_thread_put(thr);
		return -EPERM;
	}
	if(!(thr->sig_mask & (1 << signal)))
		tm_signal_send_thread(thr, signal);
	tm_thread_put(thr);
	return 0;
}

int sys_sigact(int sig, const struct sigaction *act, struct sigaction *oact)
{
	if(oact)
		memcpy(oact, (void *)&current_process->signal_act[sig], sizeof(struct sigaction));
	if(!act)
		return 0;
	/* Set actions TODO: implement all the flags */
	if(act->sa_flags & SA_SIGINFO)
		return -ENOTSUP;
	memcpy((void *)&current_process->signal_act[sig], act, sizeof(struct sigaction));
	if(act->sa_flags & SA_RESETHAND) {
		current_process->signal_act[sig]._sa_func._sa_handler=0;
		current_process->signal_act[sig].sa_flags |= SA_NODEFER;
	}
	return 0;
}

int sys_sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset)
{
	sigset_t old = current_process->global_sig_mask;
	if(oset) *oset = old;
	if(!set) {
		return 0;
	}
	sigset_t nm[3];
	sigset_t ns = *set;
	/* it's actually faster to do all three calculations and then only pick one
	 * than to setup a jump table using a switch */
	nm[SIG_UNBLOCK] = old & ~ns;
	nm[SIG_BLOCK]   = old | ns;
	nm[SIG_SETMASK] = ns;
	current_process->global_sig_mask = nm[how];
	return 0;
}

int tm_signal_will_be_fatal(struct thread *t, int sig)
{
	/* will the signal be fatal? Well, if it's SIGKILL then....yes */
	if(sig == SIGKILL) return 1;
	/* if there is a user-space handler, then it will be called, and
	 * so will not be fatal (probably) */
	if(t->process->signal_act[t->signal]._sa_func._sa_handler)
		return 0;
	/* of the default handlers, these signals don't kill the process */
	if(sig == SIGUSLEEP || sig == SIGISLEEP || sig == SIGSTOP || sig == SIGCHILD)
		return 0;
	return 1;
}

int tm_thread_got_signal(struct thread *t)
{
	if(!t->signal)
		return 0;
	if(t->signal == SIGKILL)
		return 1;
	if((addr_t)t->process->signal_act[t->signal]._sa_func._sa_handler == SIG_IGN)
		return 0;
	if(t->process->global_sig_mask & (1 << t->signal))
		return 0;
	if(t->sig_mask & (1 << t->signal))
		return 0;
	if(t->process->signal_act[t->signal].sa_flags & SA_RESTART)
		return SA_RESTART;
	return t->signal;
}

