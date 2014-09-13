/* Functions for tm_exiting processes, killing processes, and cleaning up resources.
* Copyright (c) 2012 Daniel Bittman
*/
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/fs/file.h>
#include <sea/tm/schedule.h>
#include <sea/mm/map.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/boot/init.h>

/* DESIGN: A process that calls exit() goes through a number of stages. First, exit()
 * adds the task to the kill_queue list right away, but doesn't free it yet. Once exit()
 * cleans up as much as it can, it calls set_as_dead(), which sets the tasks TF_DYING
 * flag, and sets the state to TASK_DEAD. This means that the task will not get
 * rescheduled, and tells the scheduler to "bury" the task. Since we can't risk freeing
 * the task while it's still exiting, so we wait until the scheduler is done switching away
 * from it. Once it does so, the scheduler sets the TF_BURIED flag.
 *
 * The task is now a zombie, waiting for a process to call wait() on it before it gets freed
 * up. Once wait() is called (or the kernel decides that the zombie should be ...put down...),
 * the task has its TF_KILLREADY flag set, which lets the kernel know that it can be freed.
 */

/* removes the task from the active queue of the CPU, and marks it as
 * not runnable. note: the task stays in the primary queue, so it can
 * still be accessed by its parents with a wait() call */
static __attribute__((always_inline)) inline void set_as_dead(task_t *t)
{
	assert(t);
	sub_atomic(&running_processes, 1);
	sub_atomic(&(t->cpu->numtasks), 1);
	cpu_interrupt_set(0);
	tm_raise_flag(TF_DYING);
	tqueue_remove(t->cpu->active_queue, t->activenode);
	t->state = TASK_DEAD;
}

/* only occurs when a parent wait()'s on a task, or the task is
 * inaccessable by any tasks. task is removed from the primary
 * queue. */
void __tm_remove_task_from_primary_queue(task_t *t, int locked)
{
	if(locked)
		tqueue_remove_nolock(primary_queue, t->listnode);
	else
		tqueue_remove(primary_queue, t->listnode);
	tm_process_raise_flag(t, TF_KILLREADY);
}

/* all of this stuff cannot be freed by the task itself. this
 * function gets called by the kernel background process to
 * free up the structures */
static void release_process(task_t *p)
{
	mm_destroy_task_page_directory(p);
	kfree(p->listnode);
	kfree(p->activenode);
	kfree(p->blocknode);
	kfree((void *)p->kernel_stack);
	kfree((void *)p);
}

/* kernel background process calls this, and it tries to release
 * a process, and move tasks to the kill queue */
int __KT_try_releasing_tasks()
{
	struct llistnode *cur;
	rwlock_acquire(&kill_queue->rwl, RWL_WRITER);
	/* if there's nothing in the kill_queue, return instantly */
	if(ll_is_empty(kill_queue))
	{
		rwlock_release(&kill_queue->rwl, RWL_WRITER);
		return 0;
	}
	/* check everything in the kill_queue. If a buried (zombie) task
	 * has no parent, a dead parent, or is a kernel task, then we can
	 * free it without a wait() call. */
	task_t *t=0;
	ll_for_each_entry(kill_queue, cur, task_t *, t)
	{
		/* need to check for orphaned zombie tasks */
		if(t->flags & TF_BURIED && (t != t->cpu->cur)) {
			if(t->parent == 0 
					|| t->parent->state == TASK_DEAD 
					|| (t->parent->flags & TF_KTASK) 
					|| t->parent == kernel_task)
				__tm_remove_task_from_primary_queue(t, 0);
			/* if the task is ready to be freed, then break */
			if(t->flags & TF_KILLREADY)
				break;
		}
	}
	/* make sure we can actually free it */
	if(!t || !((t->flags & TF_BURIED) && (t->flags & TF_KILLREADY)))
	{
		rwlock_release(&kill_queue->rwl, RWL_WRITER);
		return 0;
	}
	assert(cur->entry == t);
	void *node = ll_do_remove(kill_queue, cur, 1);
	assert(node == cur);
	/* return 1 if there are still tasks to free */
	int ret = 0;
	if(!ll_is_empty(kill_queue))
		ret = 1;
	rwlock_release(&kill_queue->rwl, RWL_WRITER);
	/* and finally, free the process structs */
	int pid = t->pid;
	release_process(t);
	/* free the node of the kill queue */
	kfree(cur);
	return pid;
}

void tm_process_suicide()
{
	/* we have to be a bit careful. If we're a kernel thread that
	 * uses a user-area stack (created during boot), then we need
	 * to actually do a system call in order to switch stacks */
	/* HACK: see kernel/tm/kthread.c:kthread_create */
	if((current_task->flags & TF_FORK_COPIEDUSER) && (current_task->flags & TF_KTASK)) {
		tm_switch_to_user_mode();
		u_exit(-9);
	} else {
		tm_exit(-9);
	}
}

/* just....fucking....DIE */
void tm_kill_process(unsigned int pid)
{
	if(pid == 0) return;
	task_t *task = tm_get_process_by_pid(pid);
	if(!task) {
		printk(KERN_WARN, "tm_kill_process recieved invalid PID\n");
		return;
	}
	task->state = TASK_SUICIDAL;
	task->sigd = 0; /* lol signals */
	if(task == current_task)
	{
		for(;;) tm_schedule();
	}
}

void tm_exit(int code)
{
	if(!current_task || current_task->pid == 0) 
		panic(PANIC_NOSYNC, "kernel tried to tm_exit");
	task_t *t = (task_t *)current_task;
	/* Get ready to tm_exit */
	assert(t->thread->magic == THREAD_MAGIC);
	ll_insert(kill_queue, (void *)t);
	tm_raise_flag(TF_EXITING);
	tm_remove_process_from_alarm(t);
	if(code != -9) 
		t->exit_reason.cause = 0;
	t->exit_reason.ret = code;
	t->exit_reason.pid = t->pid;
	/* Clear out system resources */
	mm_free_thread_specific_directory();
	/* tell our parent that we're dead */
	if(t->parent)
		tm_do_send_signal(t->parent->pid, SIGCHILD, 1);
	if(!sub_atomic(&t->thread->count, 1))
	{
		/* we're the last thread to share this data. Clean it up */
		fs_close_all_files(t);
		if(t->thread->root)
			vfs_iput(t->thread->root);
		if(t->thread->pwd)
			vfs_iput(t->thread->pwd);
		mutex_destroy(&t->thread->files_lock);
		mm_destroy_all_mappings(t);
		ll_destroy(&(t->thread->mappings));
		mutex_destroy(&t->thread->map_lock);
		
		void *addr = t->thread;
		t->thread = 0;
		kfree(addr);
	}
	/* don't do this while the state is dead, as we may step on the toes of waitpid.
	 * this fixes all tasks that are children of current_task, or are waiting
	 * on current_task. For those waiting, it signals the task. For those that
	 * are children, it fixes the 'parent' pointer. */
	tm_search_tqueue(primary_queue, TSEARCH_EXIT_PARENT | TSEARCH_EXIT_WAITING, 0, 0, 0, 0);
	char flag_last_page_dir_task;
	/* is this the last task to use this pd_info? */
	flag_last_page_dir_task = (sub_atomic(&pd_cur_data->count, 1) == 0) ? 1 : 0;
	if(flag_last_page_dir_task) {
		/* no one else is referencing this directory. Clean it up... */
		mm_free_thread_shared_directory();
		mm_vm_unmap(PDIR_DATA, 0);
		tm_raise_flag(TF_LAST_PDIR);
	}
	set_as_dead(t);
	for(;;) tm_schedule();
}
