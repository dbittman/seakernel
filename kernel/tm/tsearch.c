/* searches a task queue for a task with specific properties. */

#include <kernel.h>
#include <task.h>
#include <tqueue.h>

#define FOUND_ACTION \
	if(action) \
		action(tmp, arg); \
	t = tmp; \
	if(count) *count++; \
	if(flags & TSEARCH_EXCLUSIVE) goto next;

/* assumes that the passed task queue contains
 * elements of task_t.
 */
task_t *search_tqueue(tqueue_t *tq, unsigned flags, unsigned long value, void (*action)(task_t *, int), int arg, int *count)
{
	int old = set_int(0);
	mutex_acquire(&tq->lock);
	struct llistnode *cur, *next;
	task_t *tmp, *t=0;
	ll_for_each_entry_safe(&tq->tql, cur, next, task_t *, tmp)
	{
		if(flags & TSEARCH_PID && tmp->pid == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_UID && (unsigned long)tmp->thread->uid == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_EUID && (unsigned long)tmp->thread->_uid == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_TTY && (unsigned long)tmp->tty == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_EUID && (unsigned long)tmp->thread->_uid == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_PARENT && (unsigned long)tmp->parent == value)
		{
			FOUND_ACTION
		}
		if(flags & TSEARCH_ENUM && !(tmp->state == TASK_DEAD && (flags & TSEARCH_ENUM_ALIVE_ONLY)) && !value--)
		{
			t = tmp;
			break;
		}
		/* special actions required by exit */
		if(flags & TSEARCH_EXIT_WAITING && tmp->waiting == current_task)
		{
			tmp->sigd = SIGWAIT;
			tmp->waiting=0;
			tmp->waiting_ret = 0;
			memcpy((void *)&tmp->we_res, (void *)&current_task->exit_reason, 
				   sizeof(current_task->exit_reason));
			tmp->we_res.pid = current_task->pid;
			task_resume(tmp);	
		}
		if(flags & TSEARCH_EXIT_PARENT && tmp->parent == current_task)
		{
			tmp->parent = 0;
			if(tmp->state == TASK_DEAD) {
				ll_maybe_reset_loop(&tq->tql, cur, next);
				move_task_to_kill_queue(tmp, 1);
			}
		}
		next:
		/* have we found something and are only looking for one thing? */
		if(t && !(flags & TSEARCH_FINDALL))
			break;
	}
	if(flags & TSEARCH_EXIT_PARENT && (current_task->parent == 0 || (current_task->parent->flags & TF_EXITING)) && (current_task->flags & TF_EXITING))
	{
		/* some more housekeeping... */
		move_task_to_kill_queue(current_task, 1);
	}
	mutex_release(&tq->lock);
	set_int(old);
	return t;
}
