#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/file.h>
#include <sea/fs/inode.h>
#include <sea/cpu/interrupt.h>
#include <sea/mm/map.h>
#include <sea/vsprintf.h>
#include <sea/fs/kerfs.h>

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
	__remove_kerfs_thread_entry(thr, "refs");
	__remove_kerfs_thread_entry(thr, "state");
	__remove_kerfs_thread_entry(thr, "flags");
	__remove_kerfs_thread_entry(thr, "system");
	__remove_kerfs_thread_entry(thr, "priority");
	__remove_kerfs_thread_entry(thr, "timeslice");
	__remove_kerfs_thread_entry(thr, "usermode_stack_end");
	__remove_kerfs_thread_entry(thr, "sig_mask");
	__remove_kerfs_thread_entry(thr, "cpuid");
	__remove_kerfs_thread_entry(thr, "blocklist");
	char dir[128];
	snprintf(dir, 128, "/dev/process/%d/%d", thr->process->pid, thr->tid);
	sys_rmdir(dir);
}

void tm_process_remove_kerfs_entries(struct process *proc)
{
	__remove_kerfs_proc_entry(proc, "heap_start");
	__remove_kerfs_proc_entry(proc, "heap_end");
	__remove_kerfs_proc_entry(proc, "flags");
	__remove_kerfs_proc_entry(proc, "refs");
	__remove_kerfs_proc_entry(proc, "cmask");
	__remove_kerfs_proc_entry(proc, "tty");
	__remove_kerfs_proc_entry(proc, "utime");
	__remove_kerfs_proc_entry(proc, "stime");
	__remove_kerfs_proc_entry(proc, "thread_count");
	__remove_kerfs_proc_entry(proc, "effective_uid");
	__remove_kerfs_proc_entry(proc, "effective_gid");
	__remove_kerfs_proc_entry(proc, "real_uid");
	__remove_kerfs_proc_entry(proc, "real_gid");
	__remove_kerfs_proc_entry(proc, "global_sig_mask");
	__remove_kerfs_proc_entry(proc, "command");
	__remove_kerfs_proc_entry(proc, "exit_reason.sig");
	__remove_kerfs_proc_entry(proc, "exit_reason.pid");
	__remove_kerfs_proc_entry(proc, "exit_reason.ret");
	__remove_kerfs_proc_entry(proc, "exit_reason.cause");
	__remove_kerfs_proc_entry(proc, "exit_reason.coredump");
	char dir[128];
	snprintf(dir, 128, "/dev/process/%d", proc->pid);
	sys_rmdir(dir);
}

static void tm_thread_destroy(unsigned long data)
{
	struct thread *thr = (struct thread *)data;
	assert(thr != current_thread);

	/* if the thread still hasn't been rescheduled, don't destroy it yet */
	assert(thr->state == THREADSTATE_DEAD);
	if(!(thr->flags & THREAD_DEAD)) {
		struct async_call *thread_cleanup_call = async_call_create(&thr->cleanup_call, 0, 
				tm_thread_destroy, data, 0);
		workqueue_insert(&__current_cpu->work, thread_cleanup_call);
		return;
	}
	tm_thread_release_kernelmode_stack(thr->kernel_stack);
	tm_process_put(thr->process); /* thread releases it's process pointer */
	tm_thread_put(thr);
}

void tm_process_wait_cleanup(struct process *proc)
{
	assert(proc != current_process);
	/* prevent this process from being "cleaned up" multiple times */
	if(!(ff_or_atomic(&proc->flags, PROCESS_CLEANED) & PROCESS_CLEANED)) {
		assert(proc->thread_count == 0);
		ll_do_remove(process_list, &proc->listnode, 0);
		if(proc->parent)
			tm_process_put(proc->parent);
		tm_process_put(proc); /* process_list releases its pointer */
	}
}

__attribute__((noinline)) static void tm_process_exit(int code)
{
	if(code != -9) 
		current_process->exit_reason.cause = 0;
	current_process->exit_reason.ret = code;
	current_process->exit_reason.pid = current_process->pid;

	/* update times */
	if(current_process->parent) {
		time_t total_utime = current_process->utime + current_process->cutime;
		time_t total_stime = current_process->stime + current_process->cstime;
		add_atomic(&current_process->parent->cutime, total_utime);
		add_atomic(&current_process->parent->cstime, total_stime);
	}

	fs_close_all_files(current_process);
	if(current_process->root)
		vfs_icache_put(current_process->root);
	if(current_process->cwd)
		vfs_icache_put(current_process->cwd);
	mutex_destroy(&current_process->files_lock);
	mm_destroy_all_mappings(current_process);
	ll_destroy(&(current_process->mappings));
	mutex_destroy(&current_process->map_lock);
	valloc_destroy(&current_process->mmf_valloc);
	mm_free_self_directory(1);
	/* TODO: free everything else? */

	/* this is done before SIGCHILD is sent out */
	or_atomic(&current_process->flags, PROCESS_EXITED);
	if(current_process->parent) {
		struct process *init = tm_process_get(0);
		assert(init);
		rwlock_acquire(&process_list->rwl, RWL_READER);
		struct process *child;
		struct llistnode *node;
		ll_for_each_entry(process_list, node, struct process *, child) {
			if(child->parent == current_process) {
				tm_process_inc_reference(init);
				child->parent = init;
				tm_process_put(current_process);
			}
		}
		rwlock_release(&process_list->rwl, RWL_READER);
		tm_signal_send_process(current_process->parent, SIGCHILD);
		tm_blocklist_wakeall(&current_process->waitlist);
		tm_process_put(init);
	}
	tm_process_put(current_process); /* fork starts us out at refs = 1 */
}

void tm_thread_do_exit(void)
{
	assert(current_thread->blocklist == 0);

	struct async_call *thread_cleanup_call = async_call_create(&current_thread->cleanup_call, 0, 
							tm_thread_destroy, (unsigned long)current_thread, 0);

	tm_thread_release_usermode_stack(current_thread, current_thread->usermode_stack_num);
	struct ticker *ticker = current_thread->alarm_ticker;
	if(ticker) {
		int old = cpu_interrupt_set(0);
		ticker_delete(ticker, &current_thread->alarm_timeout);
		current_thread->alarm_ticker = 0;
		cpu_interrupt_set(old);
	}

	tm_thread_remove_kerfs_entries(current_thread);
	if(current_process->thread_count == 1) {
		tm_process_remove_kerfs_entries(current_process);
	}

	ll_do_remove(&current_process->threadlist, &current_thread->pnode, 0);

	sub_atomic(&running_threads, 1);
	if(sub_atomic(&current_process->thread_count, 1) == 0) {
		sub_atomic(&running_processes, 1);
		tm_process_exit(current_thread->exit_code);
	}

	cpu_disable_preemption();

	mutex_acquire(&current_thread->block_mutex);
	if(current_thread->blocklist) {
		ll_do_remove(current_thread->blocklist, &current_thread->blocknode, 0);
		current_thread->blocklist = 0;
	} else {
		tqueue_remove(current_thread->cpu->active_queue, &current_thread->activenode);
	}
	mutex_release(&current_thread->block_mutex);
	sub_atomic(&current_thread->cpu->numtasks, 1);
	current_thread->state = THREADSTATE_DEAD;
	tm_thread_raise_flag(current_thread, THREAD_SCHEDULE);
	
	workqueue_insert(&__current_cpu->work, thread_cleanup_call);
	cpu_interrupt_set(0); /* don't schedule away until we get back
							 to the syscall handler! */
	cpu_enable_preemption();
}

void tm_thread_exit(int code)
{
	current_thread->exit_code = code;
	tm_thread_raise_flag(current_thread, THREAD_EXIT);
}

void sys_exit(int code)
{
	tm_thread_exit(code);
}

