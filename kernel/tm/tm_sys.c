#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/schedule.h>
#include <sea/errno.h>
/* Low-level memory allocator implementation */
int sys_sbrk(long inc)
{
	if(inc < 0 && current_process->heap_start < current_process->heap_end) {
		int dec = -inc;
		addr_t new_end = current_process->heap_end - dec;
		if(new_end < current_process->heap_start)
			new_end = current_process->heap_start;
		addr_t old_end = current_process->heap_end;
		addr_t free_start = (new_end&PAGE_MASK) + PAGE_SIZE;
		addr_t free_end = old_end&PAGE_MASK;
		while(free_start <= free_end) {
			if(mm_vm_get_map(free_start, 0, 0))
				mm_vm_unmap(free_start, 0);
			free_start += PAGE_SIZE;
		}
		current_process->heap_end = new_end;
		assert(new_end + dec == old_end);
		return old_end;
	}
	if(!inc)
		return current_process->heap_end;
	addr_t end = current_process->heap_end;
	assert(end);
	if(end + inc >= TOP_TASK_MEM)
		tm_signal_send_thread(current_thread, SIGSEGV);
	current_process->heap_end += inc;
	addr_t page = end & PAGE_MASK;
	for(;page <=(current_process->heap_end&PAGE_MASK);page += PAGE_SIZE)
		user_map_if_not_mapped(page);
	return end;
}

int sys_alarm(int dur)
{

}

int sys_isstate(pid_t pid, int state)
{
	struct process *proc = tm_process_get(pid);
	if(!proc) return -ESRCH;
	/*TODO: how do we do this for multiple threads? */
}

int sys_gsetpriority(int set, int which, int id, int val)
{
	if(set)
		return -ENOSYS;
	return current_thread->priority;
}


#if 0
void __sys_nice_search_action(task_t *t, int val)
{
	t->priority = val;
}
#endif
int sys_nice(int which, int who, int val, int flags)
{
	/* TODO: threads? */
#if 0
	if(!flags || which == PRIO_PROCESS)
	{
		if(who && (unsigned)who != current_task->pid)
			return -ENOTSUP;
		if(!flags && val < 0 && current_task->thread->effective_uid != 0)
			return -EPERM;
		/* Yes, this is correct */
		if(!flags)
			current_task->priority += -val;
		else
			current_task->priority = (-val)+1;
		return 0;
	}
	val=-val;
	val++; /* our default is 1, POSIX default is 0 */
	task_t *t = (task_t *)kernel_task;
	int c=0;
	if(which == PRIO_USER)
		tm_search_tqueue(primary_queue, TSEARCH_UID | TSEARCH_EUID | TSEARCH_FINDALL | TSEARCH_EXCLUSIVE, who, __sys_nice_search_action, val, &c);
	return c ? 0 : -ESRCH;
#endif
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

pid_t sys_get_pid()
{
	return current_process->pid;
}

pid_t sys_getppid()
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

int tm_get_gid()
{
	return current_process->real_gid;
}

int tm_get_uid()
{
	return current_process->real_uid;
}

int tm_get_egid()
{
	return current_process->effective_gid;
}

int tm_get_euid()
{
	return current_process->effective_uid;
}

int sys_times(struct tms *buf)
{
	if(buf) {
		/*TODO: threading? */
		/* buf->tms_utime = current_process->utime; */
		/* buf->tms_stime = current_process->stime; */
		/* buf->tms_cstime = current_process->t_cstime; */
		/* buf->tms_cutime = current_process->t_cutime; */
	}
	return tm_get_ticks();
}

#if 0
static void do_sys_thread_stat(struct task_stat *s, struct thread *t)
{
	assert(s && t);
	s->stime = t->stime;
	s->utime = t->utime;
	s->state = t->state;
	if(s->state != TASK_DEAD) {
		s->uid = t->thread->real_uid;
		s->gid = t->thread->real_gid;
	}
	if(t->parent) s->ppid = t->parent->pid;
	s->system = t->system;
	s->tty = t->tty;
	s->argv = t->argv;
	s->pid = t->pid;
	strncpy(s->cmd, (char *)t->command, 128);
	s->mem_usage = (t->pid && !(t->flags & TF_KTASK)) ? t->phys_mem_usage * 4 : 0;
}
#endif
/* TODO */
int sys_task_pstat(pid_t pid, struct task_stat *s)
{
	if(!s) return -EINVAL;
	//task_t *t=tm_get_process_by_pid(pid);
	//if(!t)
		return -ESRCH;
	//do_sys_task_stat(s, t);
	return 0;
}

/* TODO */
int sys_task_stat(unsigned int num, struct task_stat *s)
{
	if(!s) return -EINVAL;
	//task_t *t = tm_search_tqueue(primary_queue, TSEARCH_ENUM, num, 0, 0, 0);
	//if(!t) 
		return -ESRCH;
	//do_sys_task_stat(s, t);
	return 0;
}

