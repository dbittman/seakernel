#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <mod.h>
#include <tqueue.h>
#include <cpu.h>
#include <ll.h>
#include <atomic.h>

extern unsigned running_processes;
volatile task_t *kernel_task=0, *alarm_list_start=0;
mutex_t *alarm_mutex=0;
extern volatile page_dir_t *kernel_dir;
volatile unsigned next_pid=0;
volatile task_t *tokill=0, *end_tokill=0;
extern void do_switch_to_user_mode();
struct llist *kill_queue=0;
tqueue_t *primary_queue=0;

void init_multitasking()
{
	printk(KERN_DEBUG, "[sched]: Starting multitasking system...\n");
	/* make the kernel task */
	task_t *task = (task_t *)kmalloc(sizeof(task_t));
	/* make this the "current_task" by assigning a specific location
	 * in the page directory as the pointer to the task. */
	page_directory[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)task;
	/* alarm_mutex is aquired inside a kernel tick, so we may not schedule. */
	alarm_mutex = mutex_create(0, MT_NOSCHED);
	task->pid = next_pid++;
	task->pd = (page_dir_t *)kernel_dir;
	task->stack_end=STACK_LOCATION;
	task->kernel_stack = kmalloc(KERN_STACK_SIZE);
	task->priority = 1;
	task->magic = TASK_MAGIC;
	task->cpu = primary_cpu;
	primary_cpu->cur = task;
	kill_queue = ll_create(0);
	primary_queue = tqueue_create(0, 0);
	primary_cpu->active_queue = tqueue_create(0, 0);
	/* allocate all of the list nodes... */
	task->listnode   = (void *)kmalloc(sizeof(struct llistnode));
	task->activenode = (void *)kmalloc(sizeof(struct llistnode));
	task->blocknode  = (void *)kmalloc(sizeof(struct llistnode));
	tqueue_insert(primary_queue, (void *)task, task->listnode);
	tqueue_insert(primary_cpu->active_queue, (void *)task, task->activenode);
	kernel_task = task;
	primary_cpu->numtasks=1;
	primary_cpu->flags |= CPU_TASK;
	mutex_create((mutex_t *)&task->exlock, MT_NOSCHED);
	add_atomic(&running_processes, 1);
#if CONFIG_MODULES
	add_kernel_symbol(delay);
	add_kernel_symbol(delay_sleep);
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
 #endif
	_add_kernel_symbol((unsigned)(task_t **)&kernel_task, "kernel_task");
#endif
}

void switch_to_user_mode()
{
	/* set up the kernel stack first...*/
	set_kernel_stack(current_tss, current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	do_switch_to_user_mode();
}

task_t *get_task_pid(int pid)
{
	return search_tqueue(primary_queue, TSEARCH_PID, pid, 0, 0, 0);
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
	t->state = TASK_ISLEEP;
	if(t == current_task) {
		while(!schedule());
	}
}

void task_resume(task_t *t)
{
	t->state = TASK_RUNNING;
}

/* we set interrupts to zero here so that we may use rwlocks in a
 * (potentially) an interrupt handler */
void task_block(struct llist *list, task_t *task)
{
	__engage_idle();
	int old = set_int(0);
	task->blocklist = list;
	ll_do_insert(list, task->blocknode, (void *)task);
	tqueue_remove(((cpu_t *)task->cpu)->active_queue, task->activenode);
	if(task == current_task) task_pause(task);
	set_int(old);
}

void task_unblock(struct llist *list, task_t *t)
{
	int old = set_int(0);
	tqueue_insert(((cpu_t *)t->cpu)->active_queue, (void *)t, t->activenode);
	struct llistnode *bn = t->blocknode;
	t->blocklist = 0;
	ll_do_remove(list, bn, 0);
	task_resume(t);
	set_int(old);
}

void task_unblock_all(struct llist *list)
{
	int old = set_int(0);
	rwlock_acquire(&list->rwl, RWL_WRITER);
	struct llistnode *cur, *next;
	task_t *entry;
	ll_for_each_entry_safe(list, cur, next, task_t *, entry)
	{
		tqueue_insert(((cpu_t *)entry->cpu)->active_queue, (void *)entry, entry->activenode);
		entry->blocklist = 0;
		assert(entry->blocknode == cur);
		ll_do_remove(list, cur, 1);
		task_resume(entry);
		ll_maybe_reset_loop(list, cur, next);
	}
	rwlock_release(&list->rwl, RWL_WRITER);
	set_int(old);
}
#if 0
void move_task_cpu(task_t *t, cpu_t *cpu)
{
	if(t->cpu == cpu) panic(0, "trying to move task to it's own cpu");
	/* have to try to get the lock on the CPU when the task t isn't 
	 * running on it... */
	#warning "make this atomic"
	if(t->flags & TF_MOVECPU)
		panic(0, "trying to move task twice");
	if(t == cpu->ktask)
		panic(0, "trying to move idle task");
	t->flags |= TF_MOVECPU;
	cpu_t *oldcpu = t->cpu;
	while(1)
	{
		mutex_acquire(&oldcpu->lock);
		if(oldcpu->cur != t)
			break;
		mutex_release(&oldcpu->lock);
		asm("pause");
	}
	/* ok, we have the lock and the task */
	tqueue_remove(oldcpu->active_queue, t->activenode);
	tqueue_insert(cpu->active_queue, (void *)t, t->activenode);
	t->cpu = cpu;
	mutex_release(&oldcpu->lock);
	t->flags &= ~TF_MOVECPU;
}
#endif