/* Functions for signaling tasks (IPC)
 * signal.c: Copyright (c) 2010 Daniel Bittman
 */
#include <sea/tm/_tm.h>
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/boot/init.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/schedule.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
int __tm_handle_signal(task_t *t)
{
	int ret = TASK_RUNNING;
	t->exit_reason.sig=0;
	struct sigaction *sa = (struct sigaction *)&(t->thread->signal_act[t->sigd]);
	t->old_mask = t->sig_mask;
	t->sig_mask |= sa->sa_mask;
	if(!(sa->sa_flags & SA_NODEFER))
		t->sig_mask |= (1 << t->sigd);
	if(sa->_sa_func._sa_handler && t->sigd != SIGKILL && arch_tm_userspace_signal_initializer(t, sa));
	else if(!sa->_sa_func._sa_handler && !t->system) {
		/* Default Handlers */
		tm_process_raise_flag(t, TF_SCHED);
		switch(t->sigd)
		{
			case SIGHUP : case SIGKILL: case SIGQUIT: case SIGPIPE:
			case SIGBUS : case SIGABRT: case SIGTRAP: case SIGSEGV:
			case SIGALRM: case SIGFPE : case SIGILL : case SIGPAGE:
			case SIGINT : case SIGTERM: case SIGUSR1: case SIGUSR2:
				t->exit_reason.cause=__EXITSIG;
				t->exit_reason.sig=t->sigd;
				tm_kill_process(t->pid);
				break;
			case SIGUSLEEP:
				ret = TASK_USLEEP;
				t->tick=0;
				break;
			case SIGSTOP: 
				if(!(sa->sa_flags & SA_NOCLDSTOP) && t->parent)
					t->parent->sigd=SIGCHILD;
				t->exit_reason.cause=__STOPSIG;
				t->exit_reason.sig=t->sigd; /* Fall through */
			case SIGISLEEP:
				ret = TASK_ISLEEP; 
				t->tick=0;
				break;
			default:
				t->flags &= ~TF_SCHED;
				break;
		}
		t->sig_mask = t->old_mask;
		t->sigd = 0;
		tm_process_lower_flag(t, TF_INSIG);
	} else {
		t->sig_mask = t->old_mask;
		tm_process_lower_flag(t, TF_INSIG);
		tm_process_raise_flag(t, TF_SCHED);
	}
	return ret;
}

static int __can_send_signal(task_t *from, task_t *to)
{
	if(from->thread->real_uid && from->thread->effective_uid)
	{
		if(from->thread->real_uid == to->thread->real_uid
			|| from->thread->real_uid == to->thread->saved_uid
			|| from->thread->effective_uid == to->thread->real_uid
			|| from->thread->effective_uid == to->thread->saved_uid)
			return 1;
		else
			return 0;
	} else {
		return 1;
	}
}
/* TODO: thread */
int tm_do_send_signal(int pid, int __sig, int p)
{
	if(!current_task)
	{
		printk(1, "Attempted to send signal %d to pid %d, but we are taskless\n",
				__sig, pid);
		return -ESRCH;
	}
	
	if(!pid && !p && current_task->thread->effective_uid && current_task->pid)
		return -EPERM;
	task_t *task = tm_get_process_by_pid(pid);
	if(!task) return -ESRCH;
	if(__sig == 127) {
		if(task->parent)
			task = task->parent;
		task->wait_again=current_task->pid;
	}
	if(__sig > 32) return -EINVAL;
	/* We may always signal ourselves */
	if(task != current_task) {
		if(!p && pid != 0 && (current_task->thread->effective_uid) && !current_task->system)
			panic(PANIC_NOSYNC, "Priority signal sent by an illegal task!");
		if(!__sig || (__sig < 32 && __can_send_signal(current_task, task) && !p))
			return -EACCES;
		if(task->state == TASK_DEAD || task->state == TASK_SUICIDAL)
			return -EINVAL;
		if(__sig < 32 && (task->sig_mask & (1<<__sig)) && __sig != SIGKILL)
			return -EACCES;
	}
	/* We need to reschedule if we signal ourselves, so that we can handle it. 
	 * The vast majority of signals below 32 require immediate handling, so we 
	 * force a reschedule. */
	task->sigd = __sig;
	if(__sig == SIGKILL)
	{
		task->exit_reason.cause=__EXITSIG;
		task->exit_reason.sig=__sig;
		task->state = TASK_RUNNING;
		tm_kill_process(pid);
	}
	if(task == current_task)
		tm_process_raise_flag(task, TF_SCHED);
	return 0;
}

int tm_send_signal(int p, int s)
{
	return tm_do_send_signal(p, s, 0);
}

void tm_set_signal(int sig, addr_t hand)
{
	assert(current_task);
	if(sig > 128)
		return;
	current_task->thread->signal_act[sig]._sa_func._sa_handler = (void (*)(int))hand;
}

void tm_remove_process_from_alarm(task_t *t)
{
	if((t->flags & TF_ALARM)) {
		tm_lower_flag(TF_ALARM);
		int old = cpu_interrupt_set(0);
		mutex_acquire(alarm_mutex);
		if(current_task->alarm_prev) current_task->alarm_prev->alarm_next = current_task->alarm_next;
		if(current_task->alarm_next) current_task->alarm_next->alarm_prev = current_task->alarm_prev;
		current_task->alarm_next = current_task->alarm_prev = 0;
		mutex_release(alarm_mutex);
		cpu_interrupt_set(old);
	}
}

/* TODO */
#if 0
int sys_alarm(int a)
{
	if(a)
	{
		int old_value = current_task->alarm_end - tm_get_ticks();
		current_task->alarm_end = a * tm_get_current_frequency() + tm_get_ticks();
		if(!(current_task->flags & TF_ALARM)) {
			/* need to clear interrupts here, because
			 * we access this inside the scheduler... */
			int old = cpu_interrupt_set(0);
			mutex_acquire(alarm_mutex);
			task_t *t = alarm_list_start, *p = 0;
			while(t && t->alarm_end < current_task->alarm_end) p = t, t = t->alarm_next;
			if(!t) {
				alarm_list_start = current_task;
				current_task->alarm_next = current_task->alarm_prev = 0;
			} else {
				if(p) p->alarm_next = current_task;
				current_task->alarm_prev = p;
				current_task->alarm_next = t;
				if(t) t->alarm_prev = current_task;
			}
			tm_raise_flag(TF_ALARM);
			mutex_release(alarm_mutex);
			cpu_interrupt_set(old);
		} else
			return old_value < 0 ? 0 : old_value;
	} else {
		tm_remove_process_from_alarm(current_task);
	}
	return 0;
}
#endif

int sys_sigact(int sig, const struct sigaction *act, struct sigaction *oact)
{
	if(oact)
		memcpy(oact, (void *)&current_task->thread->signal_act[sig], sizeof(struct sigaction));
	if(!act)
		return 0;
	/* Set actions */
	if(act->sa_flags & SA_NOCLDWAIT || act->sa_flags & SA_SIGINFO 
			|| act->sa_flags & SA_NOCLDSTOP)
		printk(0, "[sched]: sigact got unknown flags: Task %d, Sig %d. Flags: %x\n", 
			current_task->pid, sig, act->sa_flags);
	if(act->sa_flags & SA_SIGINFO)
		return -ENOTSUP;
	memcpy((void *)&current_task->thread->signal_act[sig], act, sizeof(struct sigaction));
	if(act->sa_flags & SA_RESETHAND) {
		current_task->thread->signal_act[sig]._sa_func._sa_handler=0;
		current_task->thread->signal_act[sig].sa_flags |= SA_NODEFER;
	}
	return 0;
}

int sys_sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset)
{
	if(oset)
		*oset = current_task->sig_mask;
	if(!set)
		return 0;
	sigset_t nm=0;
	switch(how)
	{
		case SIG_UNBLOCK:
			nm = current_task->sig_mask & ~(*set);
			break;
		case SIG_SETMASK:
			nm = *set;
			break;
		case SIG_BLOCK:
			nm = current_task->sig_mask | *set;
			break;
		default:
			return -EINVAL;
	}
	current_task->thread->global_sig_mask = current_task->sig_mask = nm;
	return 0;
}


int tm_signal_will_be_fatal(struct thread *t, int sig)
{
	/* will the signal be fatal? Well, if it's SIGKILL then....yes */
	if(sig == SIGKILL) return 1;
	/* if there is a user-space handler, then it will be called, and
	 * so will not be fatal (probably) */
	if(t->signal_act[t->sigd]._sa_func._sa_handler)
		return 0;
	/* of the default handlers, these signals don't kill the process */
	if(sig == SIGUSLEEP || sig == SIGISLEEP || sig == SIGSTOP || sig == SIGCHILD)
		return 0;
	return 1;
}

int tm_thread_got_signal(struct thread *t)
{
	if(kernel_state_flags & KSF_SHUTDOWN)
		return 0;
	int sn = t->cursig ? t->cursig : t->sigd;
	if(!sn) return 0;
	/* if the SA_RESTART flag is set, then return false */
	if(t->signal_act[sn].sa_flags & SA_RESTART) return 0;
	/* otherwise, return if we have a signal */
	return (sn);
}

