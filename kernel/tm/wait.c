#include <sea/subsystem.h>
#define SUBSYSTEM _SUBSYSTEM_TM
#include <sea/tm/_tm.h>

#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>

int wait_task(unsigned pid, int state)
{
	if(!state) return 0;
	if(!pid) return 0;
	if(current_task->pid) current_task->system=0;
	task_t *task = get_task_pid(pid);
	if(!task) return -ESRCH;
	current_task->waiting = task;
	if(state == -1)
		/* We wait for it to be dead. When it is, we recieve a signal, 
		 * so why loop? */
		task_pause(current_task);
	else {
		/* So, here we just wait until either the task exits or the 
		 * state becomes equal. Unfortunately we are forced to check 
		 * the state whenever we can */
		while(1) {
			task = get_task_pid(pid);
			if(!task || task->state == state)
				break;
			schedule();
		}
	}
	if(current_task->sigd != SIGWAIT && current_task->sigd)
		return -EINTR;
	return current_task->waiting_ret;
}

void get_status_int(task_t *t, int *st, int *__pid)
{
	int ret_val, sig_number;
	int status=__EXIT;
	
	sig_number = t->exit_reason.sig;
	ret_val = t->exit_reason.ret;
	status |= t->exit_reason.cause | t->exit_reason.coredump;
	if(__pid) *__pid = t->exit_reason.pid;
	
	short code=0;
	short info=0;
	if(status & __COREDUMP) code |= 0x80;
	if(status & __EXITSIG)  code |= 0x7e, info=(char)sig_number << 8;
	if(status & __STOPSIG) {
		if(status & __EXITSIG) panic(PANIC_NOSYNC, 
			"Stat of dead task returned nonsense data!");
		code |= 0x7f, info=(char)sig_number << 8;
	}
	if(status == __EXIT || status == __COREDUMP) {
		info=ret_val<<8;
	}
	if(st)
		*st = code << 16 | info;
}

int sys_waitpid(int pid, int *st, int opt)
{
	if(!pid || pid < -1)
		return -ENOSYS;
	raise_flag(TF_BGROUND);
	task_t *t=0;
	if(pid == -1) {
		/* find first child */
		t = search_tqueue(primary_queue, TSEARCH_PARENT, (addr_t)current_task, (void (*)(task_t *, int))0, 0, 0);
	} else
		t = get_task_pid(pid);
	top:
	
	if(!t || t->parent != current_task) {
		lower_flag(TF_BGROUND);
		return -ECHILD;
	}
	
	if(current_task->sigd && 
		((struct sigaction *)&(current_task->thread->signal_act
		[current_task->sigd]))->_sa_func._sa_handler && !(current_task->thread->signal_act
	[current_task->sigd].sa_flags & SA_RESTART)) {
		lower_flag(TF_BGROUND);
		return -EINTR;
	}
	
	if(t->state != TASK_DEAD) {
		if(!(opt & WNOHANG)) {
			schedule();
			goto top;
		}
		lower_flag(TF_BGROUND);
		return 0;
	}
	int code, gotpid;
	get_status_int(t, &code, &gotpid);
	if(pid == -1) __tm_move_task_to_kill_queue(t, 0);
	if(st)
		*st = code;
	lower_flag(TF_BGROUND);
	return gotpid;
}

int sys_waitagain()
{
	return (current_task->wait_again ? 
		sys_waitpid(current_task->wait_again, 0, 0)
		: 0);
}

int sys_wait3(int *a, int b, int *c)
{
	return sys_waitpid(-1, a, b);
}
