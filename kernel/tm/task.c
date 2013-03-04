#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <elf.h>
#include <tqueue.h>
volatile task_t *kernel_task=0, *alarm_list_start=0;
#if !(CONFIG_SMP)
volatile task_t *current_task=0;
#endif
extern volatile page_dir_t *kernel_dir;
volatile unsigned next_pid=0;
volatile task_t *tokill=0, *end_tokill=0;
extern void do_switch_to_user_mode();
struct llist *kill_queue=0;
tqueue_t *primary_queue=0, *active_queue=0;

void init_multitasking()
{
	printk(KERN_DEBUG, "[sched]: Starting multitasking system...\n");
	task_t *task = (task_t *)kmalloc(sizeof(task_t));
	if(!task)
		panic(PANIC_NOSYNC, "Unable to allocate memory for tasking?");
	task->pid = next_pid++;
	task->pd = (page_dir_t *)kernel_dir;
	task->stack_end=STACK_LOCATION;
	task->kernel_stack = kmalloc(KERN_STACK_SIZE+8);
	task->priority = 1;
	task->magic = TASK_MAGIC;
	kill_queue = ll_create(0);
	primary_queue = tqueue_create(0, 0);
	active_queue = tqueue_create(0, 0);
	task->listnode = tqueue_insert(primary_queue, (void *)task);
	task->activenode = tqueue_insert(active_queue, (void *)task);
	kernel_task = task;
	set_current_task_dp(task, 0);
#if CONFIG_MODULES
	add_kernel_symbol(delay);
	add_kernel_symbol(delay_sleep);
	add_kernel_symbol(schedule);
	add_kernel_symbol(schedule);
	add_kernel_symbol(run_scheduler);
	add_kernel_symbol(exit);
	add_kernel_symbol(sys_setsid);
	add_kernel_symbol(do_fork);
	add_kernel_symbol(kill_task);
	add_kernel_symbol(do_send_signal);
	add_kernel_symbol(dosyscall);
#if CONFIG_SMP
	add_kernel_symbol(get_cpu);
#else
	_add_kernel_symbol((unsigned)(task_t **)&current_task, "current_task");
#endif
	_add_kernel_symbol((unsigned)(task_t **)&kernel_task, "kernel_task");
#endif
}

void switch_to_user_mode()
{
	/* set up the kernel stack first...*/
	set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	do_switch_to_user_mode();
}

task_t *get_task_pid(int pid)
{
	return search_tqueue(primary_queue, TSEARCH_PID, pid, 0, 0);
}

int times(struct tms *buf)
{
	if(buf) {
		buf->tms_utime = current_task->utime;
		buf->tms_stime = current_task->stime;
		buf->tms_cstime = current_task->t_cstime;
		buf->tms_cutime = current_task->t_cutime;
	}
	return ticks;
}

void task_pause(task_t *t)
{
	/* don't care what other processors do */
	cli();
	t->state = TASK_ISLEEP;
	schedule();
	sti();
}

void task_resume(task_t *t)
{
	t->state = TASK_RUNNING;
}

void task_block(struct llist *list, task_t *task)
{
	task->blocklist = list;
	task->blocknode = ll_insert(list, (void *)task);
	tqueue_remove(active_queue, task->activenode);
	task->activenode = 0;
	if(task == current_task) task_pause(task);
}

void task_unblock(struct llist *list, task_t *t)
{
	t->activenode = tqueue_insert(active_queue, (void *)t);
	struct llistnode *bn = t->blocknode;
	t->blocknode = 0;
	t->blocklist = 0;
	ll_remove(list, bn);
	task_resume(t);
}

void task_unblock_all(struct llist *list)
{
	rwlock_acquire(&list->rwl, RWL_WRITER);
	struct llistnode *cur, *next;
	task_t *entry;
	ll_for_each_entry_safe(list, cur, next, task_t *, entry)
	{
		entry->activenode = tqueue_insert(active_queue, (void *)entry);
		entry->blocklist = 0;
		entry->blocknode = 0;
		ll_do_remove(list, cur, 1);
		task_resume(entry);
		ll_maybe_reset_loop(list, cur, next);
	}
	rwlock_release(&list->rwl, RWL_WRITER);
}
