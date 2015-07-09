#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
static void tm_thread_destroy(unsigned long data)
{
	struct thread *thr = (struct thread *)data;
	kprintf("got cleanup call: TID=%d\n", thr->tid);

	kfree(thr->kernel_stack);
	kfree(thr);
}

void tm_process_wait_cleanup(struct process *proc)
{
	kprintf("tm_process_wait_cleanup called\n");
	hash_table_delete_entry(process_table, &proc->pid, sizeof(proc->pid), 1);
	ll_do_remove(process_list, &proc->listnode, 0);
	mm_destroy_directory(&proc->vmm_context);
	kfree(proc);
}

/* TODO: ref count processes and threads */
void tm_process_exit(int code)
{
	/* TODO: what? */
	if(code != -9) 
		current_process->exit_reason.cause = 0;
	current_process->exit_reason.ret = code;
	current_process->exit_reason.pid = current_process->pid;

	/* TODO */
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
	/* TODO: parent inherits children */
}

void tm_thread_exit(int code)
{
	assert(current_thread->blocklist == 0);

	struct async_call *thread_cleanup_call = async_call_create(0, 0, 
							tm_thread_destroy, (unsigned long)current_thread, 0);

	sub_atomic(&running_threads, 1);
	if(sub_atomic(&current_process->thread_count, 1) == 0) {
		sub_atomic(&running_processes, 1);
		tm_process_exit(code);
	}

	cpu_interrupt_set(0);
	cpu_disable_preemption();

	ll_do_remove(&current_process->threadlist, &current_thread->pnode, 0);
	hash_table_delete_entry(thread_table, &current_thread->tid, sizeof(current_thread->tid), 1);
	tqueue_remove(current_thread->cpu->active_queue, &current_thread->activenode);
	current_thread->state = THREAD_DEAD;
	
	workqueue_insert(&__current_cpu->work, thread_cleanup_call);
	cpu_enable_preemption();
	tm_schedule();
}

void sys_exit(int code)
{
	tm_thread_exit(code);
	panic(0, "tried to return from exit");
}

