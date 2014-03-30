#include <sea/subsystem.h>
#define SUBSYSTEM _SUBSYSTEM_TM
#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <sea/loader/symbol.h>
#include <sea/tm/tqueue.h>
#include <cpu.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>

volatile task_t *kernel_task=0, *alarm_list_start=0;
mutex_t *alarm_mutex=0;
volatile unsigned next_pid=0;
struct llist *kill_queue=0;
tqueue_t *primary_queue=0;

/* create the bare task structure. This needs to be then populated with all the data
 * required for an actual process */
task_t *tm_task_create()
{
	task_t *task = (task_t *)kmalloc(sizeof(task_t));
	task->kernel_stack = (addr_t)kmalloc(KERN_STACK_SIZE);
	task->magic = TASK_MAGIC;
	/* allocate all of the list nodes... */
	task->listnode   = (void *)kmalloc(sizeof(struct llistnode));
	task->activenode = (void *)kmalloc(sizeof(struct llistnode));
	task->blocknode  = (void *)kmalloc(sizeof(struct llistnode));
	return task;
}

struct thread_shared_data *tm_thread_data_create()
{
	struct thread_shared_data *thread = (void *)kmalloc(sizeof(struct thread_shared_data));
	thread->magic = THREAD_MAGIC;
	thread->count = 1;
	mutex_create(&thread->files_lock, 0);
	return thread;
}

void tm_init_multitasking()
{
	printk(KERN_DEBUG, "[sched]: Starting multitasking system...\n");
	/* make the kernel task */
	task_t *task = tm_task_create();
	task->pid = next_pid++;
	task->pd = (page_dir_t *)kernel_dir;
	task->stack_end=STACK_LOCATION;
	task->priority = 1;
	task->cpu = primary_cpu;
	task->thread = tm_thread_data_create();
	/* alarm_mutex is aquired inside a kernel tick, so we may not schedule. */
	alarm_mutex = mutex_create(0, MT_NOSCHED);
	
	kill_queue = ll_create(0);
	primary_queue = tqueue_create(0, 0);
	primary_cpu->active_queue = tqueue_create(0, 0);

	tqueue_insert(primary_queue, (void *)task, task->listnode);
	tqueue_insert(primary_cpu->active_queue, (void *)task, task->activenode);
	
	primary_cpu->cur = task;
	primary_cpu->ktask = task;
	primary_cpu->numtasks=1;
	/* make this the "current_task" by assigning a specific location
	 * in the page directory as the pointer to the task. */
	arch_tm_set_current_task_marker((addr_t *)kernel_dir, (addr_t)task);
	kernel_task = task;
	/* this is the final thing to allow the system to begin scheduling
	 * once interrupts are enabled */
	primary_cpu->flags |= CPU_TASK;
	
	add_atomic(&running_processes, 1);
#if CONFIG_MODULES
	loader_add_kernel_symbol(tm_delay);
	loader_add_kernel_symbol(tm_delay_sleep);
	loader_add_kernel_symbol(tm_schedule);
	loader_add_kernel_symbol(tm_exit);
	loader_add_kernel_symbol(sys_setsid);
	loader_add_kernel_symbol(tm_do_fork);
	loader_add_kernel_symbol(tm_kill_process);
	loader_add_kernel_symbol(tm_do_send_signal);
	loader_add_kernel_symbol(dosyscall);
	loader_add_kernel_symbol(tm_process_pause);
	loader_add_kernel_symbol(tm_process_resume);
	loader_add_kernel_symbol(tm_process_got_signal);
 #if CONFIG_SMP
	loader_add_kernel_symbol(cpu_get);
 #endif
	loader_do_add_kernel_symbol((addr_t)(task_t **)&kernel_task, "kernel_task");
#endif
}

void tm_set_current_task_marker(page_dir_t *space, addr_t task)
{
	arch_tm_set_current_task_marker(space, task);
}

void tm_switch_to_user_mode()
{
#warning "clean up"
	/* set up the kernel stack first...*/
	set_kernel_stack(current_tss, current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	arch_tm_switch_to_user_mode();
}

task_t *tm_get_process_by_pid(int pid)
{
	return tm_search_tqueue(primary_queue, TSEARCH_PID, pid, 0, 0, 0);
}

int sys_times(struct tms *buf)
{
	if(buf) {
		buf->tms_utime = current_task->utime;
		buf->tms_stime = current_task->stime;
		buf->tms_cstime = current_task->t_cstime;
		buf->tms_cutime = current_task->t_cutime;
	}
	return ticks;
}

/* we set interrupts to zero here so that we may use rwlocks in
 * (potentially) an interrupt handler */
void tm_add_to_blocklist_and_block(struct llist *list, task_t *task)
{
	int old = interrupt_set(0);
	task->blocklist = list;
	ll_do_insert(list, task->blocknode, (void *)task);
	tqueue_remove(((cpu_t *)task->cpu)->active_queue, task->activenode);
	tm_process_pause(task);
	assert(!interrupt_set(old));
}

void tm_add_to_blocklist(struct llist *list, task_t *task)
{
	int old = interrupt_set(0);
	task->blocklist = list;
	ll_do_insert(list, task->blocknode, (void *)task);
	tqueue_remove(((cpu_t *)task->cpu)->active_queue, task->activenode);
	task->state = TASK_ISLEEP;
	assert(!interrupt_set(old));
}

void tm_remove_from_blocklist(struct llist *list, task_t *t)
{
	int old = interrupt_set(0);
	tqueue_insert(((cpu_t *)t->cpu)->active_queue, (void *)t, t->activenode);
	struct llistnode *bn = t->blocknode;
	t->blocklist = 0;
	ll_do_remove(list, bn, 0);
	tm_process_resume(t);
	assert(!interrupt_set(old));
}

void tm_remove_all_from_blocklist(struct llist *list)
{
	int old = interrupt_set(0);
	rwlock_acquire(&list->rwl, RWL_WRITER);
	struct llistnode *cur, *next;
	task_t *entry;
	ll_for_each_entry_safe(list, cur, next, task_t *, entry)
	{
		entry->blocklist = 0;
		assert(entry->blocknode == cur);
		ll_do_remove(list, cur, 1);
		tqueue_insert(((cpu_t *)entry->cpu)->active_queue, (void *)entry, entry->activenode);
		tm_process_resume(entry);
	}
	assert(!list->num);
	rwlock_release(&list->rwl, RWL_WRITER);
	assert(!interrupt_set(old));
}

void tm_move_task_cpu(task_t *t, cpu_t *cpu)
{
	panic(0, "NO!");
	assert(t && cpu);
	if(t->cpu == cpu) panic(0, "trying to move task to it's own cpu");
	assert(t != current_task);
	/* have to try to get the lock on the CPU when the task t isn't 
	 * running on it... */
	if(t->flags & TF_MOVECPU)
		panic(0, "trying to move task twice");
	if(t == cpu->ktask || t == kernel_task)
		panic(0, "trying to move idle task");
	printk(0, "moving task %d to cpu %d\n", t->pid, cpu->apicid);
	tm_process_raise_flag(t, TF_MOVECPU);
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
	t->cpu = cpu;
	tqueue_remove(oldcpu->active_queue, t->activenode);
	if(!t->blocklist)
		tqueue_insert(cpu->active_queue, (void *)t, t->activenode);
	
	mutex_release(&oldcpu->lock);
	tm_process_lower_flag(t, TF_MOVECPU);
}
