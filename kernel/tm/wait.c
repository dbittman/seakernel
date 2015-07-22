#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/errno.h>

static void get_status_int(struct process *t, int *st, int *__pid)
{
	int ret_val, sig_number;
	//int status = (t->state == TASK_DEAD) ? __EXIT : __STOPPED;
	int status = __EXIT;
	
	sig_number = t->exit_reason.sig;
	ret_val = t->exit_reason.ret;
	status |= t->exit_reason.cause | t->exit_reason.coredump;
	if(__pid) *__pid = t->exit_reason.pid;
	/* yeah, lots of weird posix stuff going on here */
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
	if(status & __STOPPED)
		info=0, code = 0x7f;
	if(st)
		*st = code << 16 | info;
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

int sys_waitpid(int pid, int *st, int opt)
{
	if(!pid || pid < -1)
		return -ENOSYS;

	struct process *proc = 0;
	if(current_process->pid > 1) printk(0, "%d wait: %d %d\n", current_process->pid, pid, opt);
	if(pid == -1) {
		proc = __find_first_child(current_process);
	} else {
		proc = tm_process_get(pid);
	}

	if(!proc || proc->parent != current_process) {
		if(current_process->pid > 1) printk(0, "%d wait: %d %d (ECHILD)\n", current_process->pid, pid, opt);
		if(proc)
			tm_process_put(proc);
		return -ECHILD;
	}

	while(!(proc->flags & PROCESS_EXITED) && !(opt & WNOHANG)) {
		switch(tm_thread_got_signal(current_thread)) {
			case SA_RESTART:
				tm_process_put(proc);
				if(current_process->pid > 1) printk(0, "%d wait: %d %d (RESTART)\n", current_process->pid, pid, opt);
				return -ERESTART;
			case 0:
				break;
			default:
				if(current_process->pid > 1) printk(0, "%d wait: %d %d (EINTR)\n", current_process->pid, pid, opt);
				tm_process_put(proc);
				return -EINTR;
		}
		tm_schedule();
	}

	if(proc->thread_count > 0 || !(proc->flags & PROCESS_EXITED)) {
		if(current_process->pid > 1) printk(0, "%d wait: %d %d (RET 0)\n", current_process->pid, pid, opt);
		tm_process_put(proc);
		return 0;
	}
	int code, gotpid;
	get_status_int(proc, &code, &gotpid);
	tm_process_wait_cleanup(proc);
	tm_process_put(proc);
	if(st)
		*st = code;
	if(current_process->pid > 1) printk(0, "%d wait: clean: %d %d)\n", current_process->pid, gotpid, code);
	return gotpid;
}

int sys_wait3(int *a, int b, int *c)
{
	return sys_waitpid(-1, a, b);
}

