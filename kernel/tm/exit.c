
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/fs/file.h>
#include <sea/fs/inode.h>
#include <sea/fs/kerfs.h>
#include <sea/mm/map.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/vsprintf.h>
#include <stdatomic.h>
#include <sea/errno.h>
#include <sea/tm/blocking.h>

#define __remove_kerfs_thread_entry(thr,name) \
	do {\
		char file[128]; \
		snprintf(file, 128, "/dev/process/%d/%d/%s", thr->process->pid, thr->tid, name); \
		kerfs_unregister_entry(file); \
	} while(0)
#define __remove_kerfs_proc_entry(p,name) \
	do {\
		char file[128]; \
		snprintf(file, 128, "/dev/process/%d/%s", p->pid, name); \
		kerfs_unregister_entry(file); \
	} while(0)

void tm_thread_remove_kerfs_entries(struct thread *thr)
{
	__remove_kerfs_thread_entry(thr, "blocklist");
	__remove_kerfs_thread_entry(thr, "cpuid");
	__remove_kerfs_thread_entry(thr, "flags");
	__remove_kerfs_thread_entry(thr, "priority");
	__remove_kerfs_thread_entry(thr, "refs");
	__remove_kerfs_thread_entry(thr, "sig_mask");
	__remove_kerfs_thread_entry(thr, "state");
	__remove_kerfs_thread_entry(thr, "system");
	__remove_kerfs_thread_entry(thr, "timeslice");
	__remove_kerfs_thread_entry(thr, "usermode_stack_end");
	char dir[128];
	snprintf(dir, 128, "/dev/process/%d/%d", thr->process->pid, thr->tid);
	int r = sys_rmdir(dir);
	assert(!r);
}

void tm_process_remove_kerfs_entries(struct process *proc)
{
	__remove_kerfs_proc_entry(proc, "cmask");
	__remove_kerfs_proc_entry(proc, "command");
	__remove_kerfs_proc_entry(proc, "effective_gid");
	__remove_kerfs_proc_entry(proc, "effective_uid");
	__remove_kerfs_proc_entry(proc, "exit_reason.cause");
	__remove_kerfs_proc_entry(proc, "exit_reason.coredump");
	__remove_kerfs_proc_entry(proc, "exit_reason.pid");
	__remove_kerfs_proc_entry(proc, "exit_reason.ret");
	__remove_kerfs_proc_entry(proc, "exit_reason.sig");
	__remove_kerfs_proc_entry(proc, "flags");
	__remove_kerfs_proc_entry(proc, "global_sig_mask");
	__remove_kerfs_proc_entry(proc, "heap_end");
	__remove_kerfs_proc_entry(proc, "heap_start");
	__remove_kerfs_proc_entry(proc, "real_gid");
	__remove_kerfs_proc_entry(proc, "real_uid");
	__remove_kerfs_proc_entry(proc, "refs");
	__remove_kerfs_proc_entry(proc, "stime");
	__remove_kerfs_proc_entry(proc, "thread_count");
	__remove_kerfs_proc_entry(proc, "tty");
	__remove_kerfs_proc_entry(proc, "utime");
	__remove_kerfs_proc_entry(proc, "maps");
	char dir[128];
	snprintf(dir, 128, "/dev/process/%d", proc->pid);
	int r = sys_rmdir(dir);
	assertmsg(!r, "%d", (long)r);
}

static void tm_thread_destroy(unsigned long data)
{
	struct thread *thr = (struct thread *)data;
	assert(thr != current_thread);

	/* if the thread still hasn't been rescheduled, don't destroy it yet */
	assert(thr->state == THREADSTATE_DEAD);
	assertmsg(thr->flags & THREAD_DEAD,
			"tried to destroy a thread before it has scheduled away");
	tm_thread_release_stacks(thr);
	tm_process_put(thr->process); /* thread releases it's process pointer */
	tm_thread_put(thr);
}

void tm_process_wait_cleanup(struct process *proc)
{
	assert(proc != current_process);
	/* prevent this process from being "cleaned up" multiple times */
	if(!(atomic_fetch_or(&proc->flags, PROCESS_CLEANED) & PROCESS_CLEANED)) {
		assert(proc->thread_count == 0);
		linkedlist_remove(process_list, &proc->listnode);
		if(proc->parent)
			tm_process_put(proc->parent);
		tm_process_put(proc); /* process_list releases its pointer */
	}
}

__attribute__((noinline)) static void tm_process_exit(int code)
{
	spinlock_acquire(&current_thread->status_lock);
	if(code != -9) 
		current_process->exit_reason.cause = __EXIT;
	current_process->exit_reason.ret = code;
	current_process->exit_reason.pid = current_process->pid;
	spinlock_release(&current_thread->status_lock);

	/* update times */
	if(current_process->parent) {
		time_t total_utime = current_process->utime + current_process->cutime;
		time_t total_stime = current_process->stime + current_process->cstime;
		atomic_fetch_add_explicit(&current_process->parent->cutime,
				total_utime, memory_order_relaxed);
		atomic_fetch_add_explicit(&current_process->parent->cstime,
				total_stime, memory_order_relaxed);
	}
	file_close_all();
	if(current_process->root)
		vfs_icache_put(current_process->root);
	if(current_process->cwd)
		vfs_icache_put(current_process->cwd);
	mutex_destroy(&current_process->fdlock);
	mm_destroy_all_mappings(current_process);
	linkedlist_destroy(&(current_process->mappings));
	mutex_destroy(&current_process->map_lock);
	valloc_destroy(&current_process->mmf_valloc);
	mm_free_userspace();
	/* TODO: free everything else? */

	/* this is done before SIGCHILD is sent out */
	atomic_fetch_or(&current_process->flags, PROCESS_EXITED);
	if(current_process->parent) {
		struct process *init = tm_process_get(0);
		assert(init);
		
		__linkedlist_lock(process_list);
		struct process *child;
		struct linkedentry *node;
		for(node = linkedlist_iter_start(process_list);
				node != linkedlist_iter_end(process_list);
				node = linkedlist_iter_next(node)) {
			child = linkedentry_obj(node);
			if(child->parent == current_process) {
				tm_process_inc_reference(init);
				child->parent = init;
				tm_process_put(current_process);
			}
		}
		__linkedlist_unlock(process_list);
		tm_signal_send_process(current_process->parent, SIGCHILD);
		tm_blocklist_wakeall(&current_process->waitlist);
		tm_process_put(init);
	}
	tm_process_put(current_process); /* fork starts us out at refs = 1 */
}

void tm_thread_do_exit(void)
{
	assert(current_thread->held_locks == 0);
	assert(current_thread->blocklist == 0);

	struct async_call *thread_cleanup_call = async_call_create(&current_thread->cleanup_call, 0, 
							tm_thread_destroy, (unsigned long)current_thread, 0);

	struct ticker *ticker = (void *)atomic_exchange(&current_thread->alarm_ticker, NULL);
	if(ticker) {
		if(ticker_delete(ticker, &current_thread->alarm_timeout) != -ENOENT)
			tm_thread_put(current_thread);
	}

	linkedlist_remove(&current_process->threadlist, &current_thread->pnode);

	tm_thread_remove_kerfs_entries(current_thread);
	atomic_fetch_sub_explicit(&running_threads, 1, memory_order_relaxed);
	if(atomic_fetch_sub(&current_process->thread_count, 1) == 1) {
		atomic_fetch_sub_explicit(&running_processes, 1, memory_order_relaxed);
		tm_process_exit(current_thread->exit_code);
		tm_process_remove_kerfs_entries(current_process);
	}

	cpu_disable_preemption();

	assert(!current_thread->blocklist);
	tqueue_remove(current_thread->cpu->active_queue, &current_thread->activenode);
	atomic_fetch_sub_explicit(&current_thread->cpu->numtasks, 1, memory_order_relaxed);
	tm_thread_raise_flag(current_thread, THREAD_SCHEDULE);
	current_thread->state = THREADSTATE_DEAD;
	
	workqueue_insert(&__current_cpu->work, thread_cleanup_call);
	cpu_interrupt_set(0); /* don't schedule away until we get back
							 to the syscall handler! */
	cpu_enable_preemption();
}

void tm_thread_exit(int code)
{
	current_thread->exit_code = code;
	if(tm_thread_lower_flag(current_thread, THREAD_PTRACED) & THREAD_PTRACED) {
		assert(current_thread->tracer);
		tm_thread_put(current_thread->tracer);
		current_thread->tracer = 0;
	}
	tm_thread_raise_flag(current_thread, THREAD_EXIT);
}

void sys_exit(int code)
{
	tm_thread_exit(code);
}

