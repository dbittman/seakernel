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

int sys_ret_sig()
{
	/* Set our flags and jump to the saved eip */
	__super_cli();
	lock_scheduler();
	current_task->flags |= TF_RETSIG;
	//struct sigaction *sa = (struct sigaction *)&(current_task->signal_act[current_task->cur_sig]);
	/* Emulate behavior for being called on the current stack by copying the new stack back to the old one */
	asm("jmp *%0"::"r"(current_task->oip));
	for(;;);
}

void handle_signal(task_t *t, int sig)
{
	assert(t == current_task);
	t->exit_reason.sig=0;
	struct sigaction *sa = (struct sigaction *)&(t->signal_act[sig]);
	/*if(sig == SIGCHILD && sa->sa_flags & SA_NOCLDSTOP)
		return;
	*/
	t->old_mask = t->sig_mask;
	t->sig_mask |= sa->sa_mask;
	if(!(sa->sa_flags & SA_NODEFER))
		t->sig_mask |= (1 << sig);
	if(!current_task->system)
		current_task->sigd=0;
	if(sa->_sa_func._sa_handler && sig != SIGKILL && sig != SIGSEGV && !current_task->system)
	{
		/* Define all variables up front */
		void (*handler)(int) = sa->_sa_func._sa_handler;
		int ret;
		__super_cli();
		lock_scheduler();
		u32int old_stack_pointer;
		u32int old_base_pointer;
		current_task->cur_sig = sig;
		/* Backup the current stack information */
		asm("mov %%esp, %0" : "=r" (old_stack_pointer));
		asm("mov %%ebp, %0" : "=r" (old_base_pointer));
		current_task->obp = old_base_pointer;
		current_task->osp = old_stack_pointer;
		current_task->osystem = current_task->system;
		/* Switch the current stack for a new one */
		current_task->oip = current_task->kernel_stack2;
		current_task->kernel_stack2 = current_task->kernel_stack;
		current_task->kernel_stack = current_task->oip;
		set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-64));
		asm("\
			mov %0, %%esp;       \
			mov %0, %%ebp;       \
		"::"r"(current_task->kernel_stack + (KERN_STACK_SIZE-64)));
		current_task->flags &= ~TF_RETSIG;
		/* Save this location */
		current_task->oip = read_eip();
		if(current_task->flags & TF_RETSIG)
		{
			/* Switch kernel stack to main one */
			current_task->oip = current_task->kernel_stack;
			current_task->kernel_stack = current_task->kernel_stack2;
			current_task->kernel_stack2 = current_task->oip;
			asm("\
				mov %1, %%esp;       \
				mov %0, %%ebp;       \
			"::"r"(current_task->obp), "r"(current_task->osp));
			set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-64));
			/* Unset flags and return to scheduler */
			current_task->system = current_task->osystem;
			current_task->oip = current_task->obp = current_task->osp = current_task->cur_sig = 0;
			current_task->flags &= ~TF_RETSIG;
			unlock_scheduler();
			goto out;
		}
		/* Switch over to ring-3 */
		__super_cli();
		unlock_scheduler();
		force_nolock(current_task);
		asm("\
			mov %0, %%esp;       \
			mov %0, %%ebp;       \
		"::"r"(STACK_LOCATION + (STACK_SIZE-64)));
		switch_to_user_mode();
		((struct sigaction *)&(current_task->signal_act[current_task->cur_sig]))->_sa_func._sa_handler(current_task->cur_sig);
		/* syscall back into the kernel */
		asm("int $0x80":"=a"(ret):"0" (128), "b" (0), "c" (0), "d" (0), "S" (0), "D" (0));
		for(;;);
	}
	else if(!(sa->_sa_func._sa_handler && sig != SIGKILL && sig != SIGSEGV))
	{
		/* Default Handlers */
		switch(sig)
		{
			case SIGHUP : case SIGKILL: case SIGQUIT: case SIGPIPE: case SIGBUS:
			case SIGABRT: case SIGTRAP: case SIGSEGV: case SIGALRM: case SIGFPE:
			case SIGILL : case SIGPAGE: case SIGINT : case SIGTERM: case SIGUSR1: 
			case SIGUSR2:
				t->exit_reason.cause=__EXITSIG;
				t->exit_reason.sig=sig;
				kill_task(t->pid);
				break;
			case SIGUSLEEP:
				if(t->uid >= current_task->uid) {
					t->state = TASK_USLEEP;
					t->tick=0;
				}
				break;
			case SIGSTOP: 
				t->exit_reason.cause=__STOPSIG;
				t->exit_reason.sig=sig; /* Fall through */
			case SIGISLEEP:
				if(t->uid >= current_task->uid) {
					t->state = TASK_ISLEEP; 
					t->tick=0;
					force_nolock(current_task);
					task_full_uncritical();
					schedule();
				}
				break;
			
		}
	}
	out:
	t->sig_mask = t->old_mask;
}

int do_extra_sigs(task_t *task, int sig)
{
	int i;
	for(i=0;i<128 && task->sig_queue[i];i++);
	if(i >= 128)
		return -EINVAL;
	task->sig_queue[i] = sig;
	return 0;
}

int do_send_signal(int pid, int __sig, int p)
{
	if(!current_task)
	{
		printk(1, "Attempted to send signal %d to pid %d, but we are taskless\n", __sig, pid);
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
	if(__sig >= 32)
		return do_extra_sigs(task, __sig);
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
	if(act->sa_flags & SA_NOCLDWAIT || act->sa_flags & SA_SIGINFO || act->sa_flags & SA_NOCLDSTOP)
		printk(0, "Warning - Signal set with flags that this implementation does not support:\n\tTask %d, Sig %d. Flags: %x\n", current_task->pid, sig, act->sa_flags);
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
