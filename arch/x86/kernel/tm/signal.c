/* Functions for signaling tasks (IPC)
 * signal.c: Copyright (c) 2010 Daniel Bittman
 */
#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <init.h>
extern int current_hz;
extern void exec_rem_sig_wrap();
unsigned int vm_setattrib(unsigned v, short attr);

char signal_return_injector[7] = {
	0xB8,
	0x80,
	0x00,
	0x00,
	0x00,
	0xCD,
	0x80
};
#define SIGSTACK (STACK_LOCATION - (STACK_SIZE + PAGE_SIZE + 8))
void handle_signal(task_t *t)
{
	t->exit_reason.sig=0;
	struct sigaction *sa = (struct sigaction *)&(t->signal_act[t->sigd]);
	t->old_mask = t->sig_mask;
	t->sig_mask |= sa->sa_mask;
	if(!(sa->sa_flags & SA_NODEFER))
		t->sig_mask |= (1 << t->sigd);
	volatile registers_t *iret = t->regs;
	if(sa->_sa_func._sa_handler && iret)
	{
		/* user-space signal handing design:
		 * 
		 * we exploit the fact the iret pops back everything in the stack, and that
		 * we have access to those stack element. We trick iret into popping
		 * back modified values of things, after pushing what looks like the
		 * first half of a subprocedure call to the new stack location for the 
		 * signal handler.
		 * 
		 * We set the return address of this 'function call' to be a bit of 
		 * injector code, listed above, which simply does a system call (128).
		 * This syscall copies back the original interrupt stack frame and 
		 * immediately goes back to the isr common handler to perform the iret
		 * back to where we were executing before.
		 */
		memcpy((void *)&t->reg_b, (void *)iret, sizeof(registers_t));
		iret->useresp = SIGSTACK;
		iret->useresp -= STACK_ELEMENT_SIZE;
		/* push the argument (signal number) */
		*(unsigned *)(iret->useresp) = t->sigd;
		iret->useresp -= STACK_ELEMENT_SIZE;
		/* push the return address */
		*(unsigned *)(iret->useresp) = (unsigned)signal_return_injector;
		iret->eip = (unsigned)sa->_sa_func._sa_handler;
		t->cursig = t->sigd;
		t->sigd=0;
		t->flags |= TF_JUMPIN;
	}
	else if(!sa->_sa_func._sa_handler && !t->system)
	{
		/* Default Handlers */
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
				if(t->uid >= t->uid) {
					t->state = TASK_USLEEP;
					t->tick=0;
				}
				break;
			case SIGSTOP: 
				t->exit_reason.cause=__STOPSIG;
				t->exit_reason.sig=t->sigd; /* Fall through */
			case SIGISLEEP:
				if(t->uid >= t->uid) {
					t->state = TASK_ISLEEP; 
					t->tick=0;
				}
				break;
		}
		t->sig_mask = t->old_mask;
		t->sigd = 0;
		t->flags &= ~TF_INSIG;
		if(t == current_task)
			t->flags |= TF_SCHED;
	} else {
		t->sig_mask = t->old_mask;
		t->flags &= ~TF_INSIG;
		t->flags |= TF_SCHED;
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
	
	if(!pid && !p && current_task->uid && current_task->pid)
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
		if(!p && pid != 0 && (current_task->uid) && !current_task->system)
			panic(PANIC_NOSYNC, "Priority signal sent by an illegal task!");
		/* Check for permissions */
		if(!__sig || (__sig < 32 && current_task->uid > task->uid && !p))
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
	if(task == current_task) {
		task_full_uncritical();
		unlock_scheduler();
		force_schedule();
	}
	return 0;
}

int send_signal(int p, int s)
{
	lock_scheduler();
	int ret = do_send_signal(p, s, 0);
	unlock_scheduler();
	return ret;
}

void set_signal(int sig, unsigned hand)
{
	assert(current_task);
	if(sig > 128)
		return;
	current_task->signal_act[sig]._sa_func._sa_handler = (void (*)(int))hand;
}

int sys_alarm(int a)
{
	lock_scheduler();
	if(a)
	{
		current_task->alrm_count = a * current_hz;
		if(!(current_task->flags & TF_ALARM)) {
			task_t *t = alarm_list_start;
			if(!t) {
				alarm_list_start = current_task;
				current_task->alarm_next=0;
			}
			else {
				task_t *n = t->alarm_next;
				t->alarm_next = current_task;
				current_task->alarm_next = n;
			}
		}
		current_task->flags |= TF_ALARM;
	} else {
		task_t *t = current_task;
		if((t->flags & TF_ALARM)) {
			task_t *r = alarm_list_start;
			while(r && r->alarm_next != t) r = r->alarm_next;
			if(!r) {
				assert(t == alarm_list_start);
				alarm_list_start = t->alarm_next;
			} else {
				assert(r->alarm_next == t);
				r->alarm_next = t->alarm_next;
				t->alarm_next=0;
			}
			current_task->flags &= ~TF_ALARM;
		}
	}
	unlock_scheduler();
	return 0;
}

int sys_sigact(int sig, const struct sigaction *act, struct sigaction *oact)
{
	if(oact)
		memcpy(oact, (void *)&current_task->signal_act[sig], sizeof(struct sigaction));
	if(!act)
		return 0;
	/* Set actions */
	if(act->sa_flags & SA_NOCLDWAIT || act->sa_flags & SA_SIGINFO 
			|| act->sa_flags & SA_NOCLDSTOP)
		printk(0, "[sched]: sigact got unknown flags: Task %d, Sig %d. Flags: %x\n", 
			current_task->pid, sig, act->sa_flags);
	if(act->sa_flags & SA_SIGINFO)
		return -ENOTSUP;
	memcpy((void *)&current_task->signal_act[sig], act, sizeof(struct sigaction));
	if(act->sa_flags & SA_RESETHAND) {
		current_task->signal_act[sig]._sa_func._sa_handler=0;
		current_task->signal_act[sig].sa_flags |= SA_NODEFER;
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
	current_task->global_sig_mask = current_task->sig_mask = nm;
	return 0;
}
