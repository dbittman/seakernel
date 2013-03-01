#include <kernel.h>
#include <memory.h>
#include <task.h>

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
		task_pause(task);
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
	int ret = current_task->waiting_ret;
	return ret;
}

int get_status_int(int pid, int *st, int *__pid)
{
	int ret_val = 0, sig_number=0;
	int status=__EXIT;
	int res = get_exit_status(pid, &status, &ret_val, &sig_number, __pid);
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
	return res;
}

int sys_waitpid(int pid, int *st, int opt)
{
	if(!pid || pid < -1)
		return -ENOSYS;
	task_t *t=kernel_task;
	if(pid == -1) {
		/* find first child */
		t = search_tqueue(primary_queue, TSEARCH_PARENT, (unsigned)current_task, (void (*)(task_t *, int))0, 0);
		if(!t && !current_task->exlist)
			return -ECHILD;
	}
	top:
	if(current_task->sigd && 
	((struct sigaction *)&(current_task->signal_act
	[current_task->sigd]))->_sa_func._sa_handler)
		return -EINTR;
	t = (pid == -1 ? 0 : get_task_pid(pid));
	if(t) {
		if(!(opt & 1)) {
			schedule();
			goto top;
		}
		return 0;
	}
	int code, gotpid, res;
	if(current_task->exlist)
		res = get_status_int(pid, &code, &gotpid);
	else {
		if(!(opt & 1)) {
			schedule();
			goto top;
		}
		return 0;
	}
	if(res) {
		if(!(opt & 1)) {
			schedule();
			goto top;
		}
		return 0;
	} else if(pid == -1){
		ex_stat *es;
		if((es=current_task->exlist)) {
			lock_scheduler();
			current_task->exlist = current_task->exlist->next;
			unlock_scheduler();
			kfree(es);
		}
	}
	if(st) 
		*st = code;
	return gotpid;
}

int sys_waitagain()
{
	if(current_task->wait_again)
		return sys_waitpid(current_task->wait_again, 0, 0);
	return 0;
}

int sys_wait3(int *a, int b, int *c)
{
	return sys_waitpid(-1, a, b);
}
