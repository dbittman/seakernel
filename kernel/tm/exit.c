/* Functions for exiting processes, killing processes, and cleaning up resources.
* Copyright (c) 2012 Daniel Bittman
*/
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>

static __attribute__((always_inline)) inline void set_as_dead(task_t *t)
{
	assert(t);
	sub_atomic(&running_processes, 1);
	sub_atomic(&(((cpu_t *)t->cpu)->numtasks), 1);
	set_int(0);
	raise_flag(TF_DYING);
	tqueue_remove(((cpu_t *)t->cpu)->active_queue, t->activenode);
	t->state = TASK_DEAD;
}

void __tm_move_task_to_kill_queue(task_t *t, int locked)
{
	if(locked)
		tqueue_remove_nolock(primary_queue, t->listnode);
	else
		tqueue_remove(primary_queue, t->listnode);
	raise_task_flag(t, TF_KILLREADY);
}

int __KT_try_releasing_tasks()
{
	struct llistnode *cur;
	rwlock_acquire(&kill_queue->rwl, RWL_WRITER);
	if(ll_is_empty(kill_queue))
	{
		rwlock_release(&kill_queue->rwl, RWL_WRITER);
		return 0;
	}
	task_t *t=0;
	ll_for_each_entry(kill_queue, cur, task_t *, t)
	{
		/* need to check for orphaned zombie tasks */
		if(t->flags & TF_BURIED && (t != ((cpu_t *)t->cpu)->cur)) {
			if(t->parent == 0 || t->parent->state == TASK_DEAD || (t->parent->flags & TF_KTASK) || t->parent == kernel_task)
				__tm_move_task_to_kill_queue(t, 0);
			if(t->flags & TF_KILLREADY)
				break;
		}
	}
	if(!t || !((t->flags & TF_BURIED) && (t->flags & TF_KILLREADY)))
	{
		rwlock_release(&kill_queue->rwl, RWL_WRITER);
		return 0;
	}
	assert(cur->entry == t);
	void *node = ll_do_remove(kill_queue, cur, 1);
	assert(node == cur);
	int ret = 0;
	if(!ll_is_empty(kill_queue))
		ret = 1;
	rwlock_release(&kill_queue->rwl, RWL_WRITER);
	release_task(t);
	kfree(cur);
	return ret;
}

void release_task(task_t *p)
{
	destroy_task_page_directory(p);
	kfree(p->listnode);
	kfree(p->activenode);
	kfree(p->blocknode);
	kfree((void *)p->kernel_stack);
	kfree((void *)p);
}

void task_suicide()
{
	exit(-9);
}

void kill_task(unsigned int pid)
{
	if(pid == 0) return;
	task_t *task = get_task_pid(pid);
	if(!task) {
		printk(KERN_WARN, "kill_task recieved invalid PID\n");
		return;
	}
	task->state = TASK_SUICIDAL;
	task->sigd = 0; /* fuck your signals */
	if(task == current_task)
	{
		for(;;) schedule();
	}
}

void exit(int code)
{
	if(!current_task || current_task->pid == 0) 
		panic(PANIC_NOSYNC, "kernel tried to exit");
	task_t *t = (task_t *)current_task;
	/* Get ready to exit */
	assert(t->thread->magic == THREAD_MAGIC);
	ll_insert(kill_queue, (void *)t);
	raise_flag(TF_EXITING);
	if(code != -9) 
		t->exit_reason.cause = 0;
	t->exit_reason.ret = code;
	t->exit_reason.pid = t->pid;
	/* Clear out system resources */
	free_thread_specific_directory();
	/* tell our parent that we're dead */
	if(t->parent)
		do_send_signal(t->parent->pid, SIGCHILD, 1);
	if(!sub_atomic(&t->thread->count, 1))
	{
		/* we're the last thread to share this data. Clean it up */
		close_all_files(t);
		if(t->thread->root)iput(t->thread->root);
		if(t->thread->pwd) iput(t->thread->pwd);
		mutex_destroy(&t->thread->files_lock);
		void *addr = t->thread;
		t->thread = 0;
		kfree(addr);
	}
	/* don't do this while the state is dead, as we may step on the toes of waitpid.
	 * this fixes all tasks that are children of current_task, or are waiting
	 * on current_task. For those waiting, it signals the task. For those that
	 * are children, it fixes the 'parent' pointer. */
	search_tqueue(primary_queue, TSEARCH_EXIT_PARENT | TSEARCH_EXIT_WAITING, 0, 0, 0, 0);
	char flag_last_page_dir_task;
	/* is this the last task to use this pd_info? */
	flag_last_page_dir_task = (sub_atomic(&pd_cur_data->count, 1) == 0) ? 1 : 0;
	if(flag_last_page_dir_task) {
		/* no one else is referencing this directory. Clean it up... */
		free_thread_shared_directory();
		vm_unmap(PDIR_DATA);
		raise_flag(TF_LAST_PDIR);
	}
	set_as_dead(t);
	for(;;) schedule();
}
