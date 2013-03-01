/* Functions for exiting processes, killing processes, and cleaning up resources.
* Copyright (c) 2012 Daniel Bittman
*/
#include <kernel.h>
#include <memory.h>
#include <task.h>
extern struct llist *kill_queue;

void clear_resources(task_t *t)
{
	clear_mmfiles(t, (t->flags&TF_EXITING) ? 1 : 0);
}

void set_as_dead(task_t *t)
{
	assert(t);
	t->state = TASK_DEAD;
	tqueue_remove(primary_queue, t->listnode);
	t->listnode = ll_insert(kill_queue, (void *)t);
	/* Add to death */
	__engage_idle();
}

int __KT_try_releasing_tasks()
{
	task_t *t = ll_remove_head(kill_queue);
	if(t) release_task(t);
	return !ll_is_empty(kill_queue);
}

void release_task(task_t *p)
{
	/* This is everything that the task itself cannot release. 
	 * The kernel cleans up what little is left nicely */
	assert(current_task == kernel_task);
	assert(p != (task_t *)current_task);
	
	/* Is this page table marked as unreferenced? */
	if(p->flags & TF_LAST_PDIR) {
		/* Free the accounting page table */
		pm_free_page(p->pd[PAGE_DIR_IDX(PDIR_DATA/PAGE_SIZE)] & PAGE_MASK);
	}
	/* Free the self-ref'ing page table */
	pm_free_page(p->pd[1022] & PAGE_MASK);
	kfree(p->pd);
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
		schedule();
}

int get_exit_status(int pid, int *status, int *retval, int *signum, int *__pid)
{
	if(!pid) 
		return -1;
	ex_stat *es;
	lock_scheduler();
	ex_stat *ex = current_task->exlist;
	if(pid != -1) 
		while(ex && ex->pid != (unsigned)pid) ex=ex->next;
	es=ex;
	unlock_scheduler();
	if(es)
	{
		if(__pid) *__pid = es->pid;
		*status |= es->coredump | es->cause;
		*retval = es->ret;
		*signum = es->sig;
		return 0;
	}
	return 1;
}

void add_exit_stat(task_t *t, ex_stat *e)
{
	e->pid = current_task->pid;
	ex_stat *n = (ex_stat *)kmalloc(sizeof(*e));
	memcpy(n, e, sizeof(*e));
	n->prev=0;
	lock_scheduler();
	ex_stat *old = t->exlist;
	if(old) old->prev = n;
	t->exlist = n;
	n->next=old;
	unlock_scheduler();
}

void exit(int code)
{
	if(!current_task || current_task->pid == 0) 
		panic(PANIC_NOSYNC, "kernel tried to exit");
	task_t *t = (task_t *)current_task;
	/* Get ready to exit */
	raise_flag(TF_EXITING);
	if(code != -9) t->exit_reason.cause = 0;
	t->exit_reason.ret = code;
	add_exit_stat((task_t *)t->parent, (ex_stat *)&t->exit_reason);
	/* Clear out system resources */
	free_stack(); /* free up memory that is thread-specific */
	clear_resources(t);
	close_all_files(t);
	iput(t->root);
	iput(t->pwd);
	/* Send out some signals */
	set_int(0);
	mutex_acquire(&primary_queue->lock);
	struct llistnode *cur;
	task_t *tmp;
	ll_for_each_entry(&primary_queue->tql, cur, task_t *, tmp)
	{
		if(tmp->parent == t)
			tmp->parent=0;
		if(tmp->waiting == t)
		{
			tmp->sigd = SIGWAIT;
			tmp->waiting=0;
			tmp->waiting_ret = code;
			memcpy((void *)&tmp->we_res, (void *)&t->exit_reason, 
				sizeof(t->exit_reason));
			tmp->we_res.pid = t->pid;
			task_resume(tmp);
		}
	}
	mutex_release(&primary_queue->lock);
	if(t->parent) {
		do_send_signal(t->parent->pid, SIGCHILD, 1);
		t->parent = t->parent->parent;
	}
	/* Lock out everything and modify the linked-lists */
	lock_scheduler();
	ex_stat *ex = t->exlist;
	while(ex) {
		ex_stat *n = ex->next;
		kfree(ex);
		ex=n;
	}
	unlock_scheduler();
	raise_flag(TF_DYING);
	set_as_dead(t);
	char flag_last_page_dir_task=0;
	mutex_acquire(&pd_cur_data->lock);
	flag_last_page_dir_task = --pd_cur_data->count == 0 ? 1 : 0;
	mutex_release(&pd_cur_data->lock);
	if(flag_last_page_dir_task) {
		/* no one else is referencing this directory. Clean it up... */
		self_free(0);
		vm_unmap(PDIR_DATA);
		raise_flag(TF_LAST_PDIR);
	}
	vm_switch((page_dir_t *)kernel_dir);
	schedule();
	panic(PANIC_NOSYNC, "and you may ask yourself...how did I get here?");
}
