#include <sea/errno.h>
#include <sea/tm/process.h>
#include <sea/tm/ptrace.h>
#include <sea/tm/thread.h>
#include <sea/trace.h>
#include <sea/vsprintf.h>
#include <sea/cpu/ptrace_user.h>

int ptrace_read_user(struct thread *thread, struct ptrace_user *user)
{
#if ARCH_HAS_PTRACE_READ_USER
	return arch_cpu_ptrace_read_user(thread, user);
#endif
	return -ENOTSUP;
}

int ptrace_write_user(struct thread *thread, struct ptrace_user *user)
{
#if ARCH_HAS_PTRACE_WRITE_USER
	return arch_cpu_ptrace_write_user(thread, user);
#endif
	return -ENOTSUP;
}

long sys_ptrace_thread(enum __ptrace_request request, pid_t tid, void *addr, void *data)
{
	int ret = 0;
	if(request == PTRACE_TRACEME) {
		struct thread *tracer;
		tracer = tm_process_get_head_thread(current_process->parent);
		if(!tracer) {
			return -ESRCH;
		}
		tm_thread_inc_reference(tracer);
		current_thread->tracer = tracer;
		tm_thread_raise_flag(current_thread, THREAD_PTRACED);
		TRACE_MSG("ptrace", "thread %d set to be traced by %d\n",
				current_thread->tid, current_thread->tracer->tid);
		return ret;
	}
	struct thread *tracee = tm_thread_get(tid);
	if(!tracee) {
		return -ESRCH;
	}
	if(request != PTRACE_SEIZE && request != PTRACE_ATTACH && tracee->tracer != current_thread) {
		TRACE_MSG("ptrace", "thread %d tried to ptrace on %d, which it wasn't tracing!\n",
				current_thread->tid, tracee->tid);
		tm_thread_put(tracee);
		return -ESRCH;
	}
	mutex_acquire(&tracee->block_mutex);
	switch(request) {
		case PTRACE_SYSCALL:
			TRACE_MSG("ptrace", "thread %d set to STOPON_SYSCALL mode by %d\n", tracee->tid, current_thread->tid);
			tracee->tracee_flags |= TRACEE_STOPON_SYSCALL;
			tracee->process->exit_reason.cause = 0;
			tracee->process->exit_reason.sig = 0;
			tracee->state = THREADSTATE_RUNNING;
			break;
		case PTRACE_READUSER:
			TRACE_MSG("ptrace", "thread %d state read by %d\n", tracee->tid, current_thread->tid);
			if(tracee->state == THREADSTATE_STOPPED) {
				ret = arch_cpu_ptrace_read_user(tracee, data);
			} else {
				ret = -EIO;
			}
			break;
		case PTRACE_WRITEUSER:
			TRACE_MSG("ptrace", "thread %d state written to by %d\n", tracee->tid, current_thread->tid);
			if(tracee->state == THREADSTATE_STOPPED) {
				ret = arch_cpu_ptrace_write_user(tracee, data);
			} else {
				ret = -EIO;
			}
			break;
		case PTRACE_PEEKDATA:
			mm_context_read(&tracee->process->vmm_context, &ret, addr, sizeof(long));
			break;
		case PTRACE_DETACH:
			if(tm_thread_lower_flag(tracee, THREAD_PTRACED) & THREAD_PTRACED) {
				assert(tracee->tracer == current_thread);
				tm_thread_put(tracee->tracer);
				tracee->tracer = 0;
			}
			/* fall through */
		case PTRACE_CONT:
			tracee->process->exit_reason.cause = 0;
			tracee->process->exit_reason.sig = 0;
			tracee->state = THREADSTATE_RUNNING;
			break;
		default:
			printk(0, "[ptrace]: unknown ptrace request: %d\n", request);
			ret = -EINVAL;
	}
	mutex_release(&tracee->block_mutex);
	tm_thread_put(tracee);
	return ret;
}

long sys_ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data)
{
	if(request == PTRACE_TRACEME) {
		return sys_ptrace_thread(request, pid, addr, data);
	}
	struct process *proc = tm_process_get(pid);
	if(!proc) {
		return -ESRCH;
	}
	/* TODO: permissions */

	struct thread *t = tm_process_get_head_thread(proc);
	pid_t tid = 0;
	if(t)
		tid = t->tid;
	tm_process_put(proc);

	return tid ? sys_ptrace_thread(request, tid, addr, data) : -ESRCH;
}

