#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/file.h>
#include <sea/fs/inode.h>
#include <sea/cpu/interrupt.h>
#include <sea/mm/map.h>
static void tm_thread_destroy(unsigned long data)
{
	struct thread *thr = (struct thread *)data;
	kfree(thr->kernel_stack);
	tm_thread_put(thr);
}

void tm_process_wait_cleanup(struct process *proc)
{
	/* prevent this process from being "cleaned up" multiple times */
	if(!(ff_or_atomic(&proc->flags, PROCESS_CLEANED) & PROCESS_CLEANED)) {
		mm_destroy_directory(&proc->vmm_context);
		ll_do_remove(process_list, &proc->listnode, 0);
		tm_process_put(proc); /* process_list releases its pointer */
	}
}

static void tm_process_exit(int code)
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

	if(current_process->parent)
		tm_signal_send_process(current_process->parent, SIGCHILD);
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
	mm_free_self_directory();
	/* TODO: free everything else? */

	if(current_process->parent) {
		rwlock_acquire(&process_list->rwl, RWL_READER);
		struct process *child;
		struct llistnode *node;
		ll_for_each_entry(process_list, node, struct process *, child) {
			if(child->parent == current_process) {
				child->parent = current_process->parent;
				tm_process_inc_reference(current_process->parent);
				tm_process_put(current_process);
			}
		}
		rwlock_release(&process_list->rwl, RWL_READER);
		tm_process_put(current_process->parent);
	}
	or_atomic(&current_process->flags, PROCESS_EXITED);
}

void tm_thread_exit(int code)
{
	assert(current_thread->blocklist == 0);

	struct async_call *thread_cleanup_call = async_call_create(0, 0, 
							tm_thread_destroy, (unsigned long)current_thread, 0);

	tm_thread_release_usermode_stack(current_thread, current_thread->usermode_stack_num);

	sub_atomic(&running_threads, 1);
	if(sub_atomic(&current_process->thread_count, 1) == 0) {
		sub_atomic(&running_processes, 1);
		tm_process_exit(code);
		tm_process_put(current_process); /* fork starts us out at refs = 1 */
	}

	ll_do_remove(&current_process->threadlist, &current_thread->pnode, 0);
	tm_process_put(current_process); /* thread releases it's process pointer */

	cpu_disable_preemption();

	tqueue_remove(current_thread->cpu->active_queue, &current_thread->activenode);
	sub_atomic(&current_thread->cpu->numtasks, 1);
	current_thread->state = THREADSTATE_DEAD;
	tm_thread_raise_flag(current_thread, THREAD_SCHEDULE);
	
	workqueue_insert(&__current_cpu->work, thread_cleanup_call);
	cpu_enable_preemption();
}

void sys_exit(int code)
{
	tm_thread_exit(code);
}

