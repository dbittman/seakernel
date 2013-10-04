/* Functions for signaling tasks (IPC)
 * signal.c: Copyright (c) 2010 Daniel Bittman
 */
#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <init.h>

int arch_userspace_signal_initializer(task_t *t, struct sigaction *sa);
void handle_signal(task_t *t)
{
	t->exit_reason.sig=0;
	struct sigaction *sa = (struct sigaction *)&(t->thread->signal_act[t->sigd]);
	t->old_mask = t->sig_mask;
	t->sig_mask |= sa->sa_mask;
	if(!(sa->sa_flags & SA_NODEFER))
		t->sig_mask |= (1 << t->sigd);
	/* tricky short-circuit evaluation */
	if(sa->_sa_func._sa_handler && t->sigd != SIGKILL && arch_userspace_signal_initializer(t, sa));
	else if(!sa->_sa_func._sa_handler && !t->system)
	{
		/* Default Handlers */
		raise_task_flag(t, TF_SCHED);
		switch(t->sigd)
		{
			case SIGHUP : case SIGKILL: case SIGQUIT: case SIGPIPE: 
			case SIGBUS : case SIGABRT: case SIGTRAP: case SIGSEGV: 
			case SIGALRM: case SIGFPE : case SIGILL : case SIGPAGE: 
			case SIGINT : case SIGTERM: case SIGUSR1: case SIGUSR2:
				t->exit_reason.cause=__EXITSIG;
				t->exit_reason.sig=t->sigd;
				kill_task(t->pid);
				break;
			case SIGUSLEEP:
				if(t->thread->uid >= t->thread->uid) {
					t->state = TASK_USLEEP;
					t->tick=0;
				}
				break;
			case SIGSTOP: 
				if(!(sa->sa_flags & SA_NOCLDSTOP))
					t->parent->sigd=SIGCHILD;
				t->exit_reason.cause=__STOPSIG;
				t->exit_reason.sig=t->sigd; /* Fall through */
			case SIGISLEEP:
				if(t->thread->uid >= t->thread->uid) {
					t->state = TASK_ISLEEP; 
					t->tick=0;
				}
				break;
			default:
				t->flags &= ~TF_SCHED;
				break;
		}
		t->sig_mask = t->old_mask;
		t->sigd = 0;
		lower_task_flag(t, TF_INSIG);
	} else {
		t->sig_mask = t->old_mask;
		lower_task_flag(t, TF_INSIG);
		raise_task_flag(t, TF_SCHED);
	}
}

int do_send_signal(int pid, int __sig, int p)
{
	if(!current_task)
	{
		printk(1, "Attempted to send signal %d to pid %d, but we are taskless\n",
				__sig, pid);
		return -ESRCH;
	}
	
	if(!pid && !p && current_task->thread->uid && current_task->pid)
		return -EPERM;
	task_t *task = get_task_pid(pid);
	if(!task) return -ESRCH;
	if(__sig == 127) {
		if(task->parent) task = task->parent;
		task->wait_again=current_task->pid;
	}
	if(__sig > 32) return -EINVAL;
	/* We may always signal ourselves */
	if(task != current_task) {
		if(!p && pid != 0 && (current_task->thread->uid) && !current_task->system)
			panic(PANIC_NOSYNC, "Priority signal sent by an illegal task!");
		/* Check for permissions */
		if(!__sig || (__sig < 32 && current_task->thread->uid > task->thread->uid && !p))
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
		kill_task(pid);
	}
	if(task == current_task)
		raise_task_flag(task, TF_SCHED);
	return 0;
}

int send_signal(int p, int s)
{
	return do_send_signal(p, s, 0);
}

void set_signal(int sig, addr_t hand)
{
	assert(current_task);
	if(sig > 128)
		return;
	current_task->thread->signal_act[sig]._sa_func._sa_handler = (void (*)(int))hand;
}

int sys_alarm(int a)
{
	if(a)
	{
		int old_value = current_task->alarm_end - ticks;
		current_task->alarm_end = a * current_hz + ticks;
		if(!(current_task->flags & TF_ALARM)) {
			/* need to clear interrupts here, because
			 * we access this inside the scheduler... */
			int old = set_int(0);
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
			raise_flag(TF_ALARM);
			mutex_release(alarm_mutex);
			set_int(old);
		} else
			return old_value < 0 ? 0 : old_value;
	} else {
		task_t *t = current_task;
		if((t->flags & TF_ALARM)) {
			lower_flag(TF_ALARM);
			int old = set_int(0);
			mutex_acquire(alarm_mutex);
			if(current_task->alarm_prev) current_task->alarm_prev->alarm_next = current_task->alarm_next;
			if(current_task->alarm_next) current_task->alarm_next->alarm_prev = current_task->alarm_prev;
			current_task->alarm_next = current_task->alarm_prev = 0;
			mutex_release(alarm_mutex);
			set_int(old);
		}
	}
	return 0;
}

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


int signal_will_be_fatal(task_t *t, int sig)
{
	if(sig == SIGKILL) return 1;
	if(t->thread->signal_act[t->sigd]._sa_func._sa_handler) return 0;
	if(sig == SIGUSLEEP || sig == SIGISLEEP || sig == SIGSTOP || sig == SIGCHILD)
		return 0;
	return 1;
}

int got_signal(task_t *t)
{
	if(kernel_state_flags & KSF_SHUTDOWN)
		return 0;
	if(!t->sigd) return 0;
	/* if the SA_RESTART flag is set, then return false */
	if(t->thread->signal_act[t->sigd].sa_flags & SA_RESTART) return 0;
	/* otherwise, return if we have a signal */
	return (t->sigd);
}
