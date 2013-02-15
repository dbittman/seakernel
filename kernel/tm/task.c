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
tqueue_t *primary_queue=0;

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
	set_current_task_dp(task, 0);
	kernel_task = task;
	kill_queue = ll_create(0);
	primary_queue = tqueue_create(0, 0);
	kernel_task->listnode = tqueue_insert(primary_queue, (void *)kernel_task);
	
#if CONFIG_MODULES
	add_kernel_symbol(delay);
	add_kernel_symbol(delay_sleep);
	add_kernel_symbol(schedule);
	add_kernel_symbol(schedule);
	add_kernel_symbol(run_scheduler);
	add_kernel_symbol(exit);
	add_kernel_symbol(sys_setsid);
	add_kernel_symbol(fork);
	add_kernel_symbol(kill_task);
	add_kernel_symbol(__wait_flag);
	add_kernel_symbol(wait_flag_except);
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
	//lock_task_queue_reading(0);
	//task_t *task = kernel_task;
	//while(task && task->pid != (unsigned)pid)
	//	task = task->next;
	//unlock_task_queue_reading(0);
	set_int(0);
	mutex_acquire(&primary_queue->lock);
	struct llistnode *cur;
	task_t *tmp, *t=0;
	ll_for_each_entry(&primary_queue->tql, cur, task_t *, tmp)
	{
		if(tmp->pid == (unsigned)pid)
		{
			t = tmp;
			break;
		}
	}
	mutex_release(&primary_queue->lock);
	set_int(1);
	return t;
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
