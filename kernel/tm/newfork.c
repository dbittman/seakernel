#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/ll.h>

/* TODO: hash table of threads and processes */

struct thread *tm_thread_fork(int flags)
{
	struct thread *thr = kmalloc(sizeof(struct thread));
	thr->magic = THREAD_MAGIC;
	thr->tid = atomic_add(&__next_tid, 1);
	thr->flags = THREAD_FORK;
	thr->priority = current_thread->priority;
	thr->kernel_stack = kmalloc_ap(0x1000);
	thr->signal_mask = current_thread->signal_mask;
	
	return thr;
}

struct process *tm_process_copy(int flags)
{
	/* copies the current_process structure into
	 * a new one (cloning the things that need
	 * to be cloned) */
	struct process *newp = kmalloc(sizeof(struct process));
	newp->magic = PROCESS_MAGIC;
	newp->pd = mm_clone();
	newp->pid = atomic_add(&__next_pid, 1);
	newp->flags = PROCESS_FORK;
	newp->cmask = current_process->cmask;
	newp->tty = current_process->tty;
	newp->heap_start = current_process->heap_start;
	newp->heap_end = current_process->heap_end;
	newp->signal_mask = current_process->signal_mask;
	newp->signal = current_process->signal; /* TODO: do we need this, or just in threads? */
	newp->parent = current_process;
	ll_create(&newp->threadlist);
	return newp;
}

void tm_thread_add_to_process(struct thread *thr, struct process *proc)
{
	ll_do_insert(&proc->threadlist, &thr->pnode, thr);
	thr->process = proc;
	add_atomic(&proc->thread_count, 1);
}

int sys_clone(int flags)
{
	struct process *proc = current_process;
	if(flags & CLONE_PROCESS) {
		proc = tm_process_copy();
		ll_do_insert(tm_proclist, &proc->listnode, proc);
	}
	struct thread *thr = tm_thread_fork();
	tm_thread_add_to_process(thr, proc);
	thr->state = TASK_UNINTERRUPTIBLE; /* TODO: or whatever */
	tm_thread_add_to_cpu(thr);
}

