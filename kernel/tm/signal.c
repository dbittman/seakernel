/* Functions for signaling tasks (IPC)
 * signal.c: Copyright (c) 2010 Daniel Bittman
 */
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/boot/init.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>

int tm_thread_handle_signal(int signal)
{
	struct sigaction *sa = &current_process->signal_act[signal];
	current_thread->old_mask = current_thread->sig_mask;
	if(!(sa->sa_flags & SA_NODEFER))
		current_thread->sig_mask |= (1 << signal);
	if(signal != SIGKILL && sa->_sa_func._sa_handler) {
		tm_thread_raise_flag(current_thread, TF_SIGNALED);
	} else if(!current_thread->system) {
		/* Default Handlers */
		tm_thread_raise_flag(current_thread, TF_SCHED);
		switch(signal)
		{
			case SIGHUP : case SIGKILL: case SIGQUIT: case SIGPIPE:
			case SIGBUS : case SIGABRT: case SIGTRAP: case SIGSEGV:
			case SIGALRM: case SIGFPE : case SIGILL : case SIGPAGE:
			case SIGINT : case SIGTERM: case SIGUSR1: case SIGUSR2:
				/* TODO */
				//t->exit_reason.cause=__EXITSIG;
				//t->exit_reason.sig=t->sigd;
				tm_signal_send_thread(current_thread, SIGKILL);
				break;
			case SIGUSLEEP:
				//ret = TASK_USLEEP;
				//t->tick=0;
				break;
			case SIGSTOP: 
				//if(!(sa->sa_flags & SA_NOCLDSTOP) && t->parent)
				//	t->parent->sigd=SIGCHILD;
				//t->exit_reason.cause=__STOPSIG;
				//t->exit_reason.sig=t->sigd; /* Fall through */
			case SIGISLEEP:
				//ret = TASK_ISLEEP; 
				//t->tick=0;
				break;
			default:
				//t->flags &= ~TF_SCHED;
				break;
		}
	}
	
	/* RESTORE SIGNAL MASK? */

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

/* TODO: handle sigkill */

void tm_signal_send_thread(struct thread *thr, int signal)
{
	/* TODO: check if there's already a signal? */
	thr->signal = signal;
	if(thr->state == THREAD_INTERRUPTIBLE)
		tm_thread_set_state(thr, THREAD_RUNNING);
}

int tm_signal_send_process(struct process *proc, int signal)
{
	rwlock_acquire(&proc->threadlist.rwl, RWL_READER);
	struct llistnode *node;
	struct thread *thr;
	/* find the first thread that is willing to handle the signal */
	ll_for_each_entry(&proc->threadlist, node, struct thread *, thr) {
		if(!(thr->sig_mask & (1 << signal))) {
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
	if(pid == 0)
		return -EINVAL;
	struct process *proc = tm_process_get(pid);
	if(!proc)
		return -ESRCH;
	if(!__can_send_signal(current_process, proc, signal)) {
		tm_process_put(proc);
		return -EPERM;
	}
	if(!(proc->global_sig_mask & (1 << signal)))
		tm_signal_send_process(proc, signal);
	tm_process_put(proc);
	return 0;
}

/* TODO: add to syscall table */
int sys_kill_thread(pid_t tid, int signal)
{
	struct thread *thr = tm_thread_get(tid);
	if(!thr)
		return -ESRCH;
	if(thr->process != current_process)
		return -EPERM;
	if(!(thr->sig_mask & (1 << signal)))
		tm_signal_send_thread(thr, signal);
	return 0;
}

/* TODO: alarm */

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
	if(oset)
		*oset = current_process->global_sig_mask;
	if(!set)
		return 0;
	sigset_t nm=0;
	switch(how)
	{
		case SIG_UNBLOCK:
			nm = current_process->global_sig_mask & ~(*set);
			break;
		case SIG_SETMASK:
			nm = *set;
			break;
		case SIG_BLOCK:
			nm = current_process->global_sig_mask | *set;
			break;
		default:
			return -EINVAL;
	}
	current_process->global_sig_mask = nm;
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
	/* SA_RESTART? */
	return t->signal;
}

