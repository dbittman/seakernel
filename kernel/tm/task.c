#include <kernel.h>
#include <memory.h>
#include <task.h>
volatile task_t *kernel_task=0, *alarm_list_start=0;
#if !(CONFIG_SMP)
volatile task_t *current_task=0;
#endif
extern volatile page_dir_t *kernel_dir;
volatile unsigned next_pid=0;
volatile task_t *tokill=0, *end_tokill=0;
extern void do_switch_to_user_mode();

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
	create_mutex(&scheding);
}

void switch_to_user_mode()
{
	set_kernel_stack(current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	do_switch_to_user_mode();
}

task_t *get_task_pid(int pid)
{
	lock_scheduler();
	task_t *task = kernel_task;
	while(task && task->pid != (unsigned)pid)
		task = task->next;
	unlock_scheduler();
	return task;
}

void freeze_all_tasks()
{
	if(current_task->uid != 0) return;
	serial_puts(0, "Freezing all tasks...\n");
	super_cli();
	task_t *t = (task_t *)kernel_task->next;
	while(t)
	{
		if(t != current_task) {
			t->last = t->state;
			t->state = TASK_FROZEN;
		}
		t = t->next;
	}
	super_sti();
}

void unfreeze_all_tasks()
{
	if(current_task->uid != 0) return;
	serial_puts(0, "Unfreezing all tasks...\n");
	super_cli();
	task_t *t = (task_t *)kernel_task->next;
	while(t)
	{
		if(t != current_task) {
			t->state = t->last;
		}
		t = t->next;
	}
	super_sti();
}

void kill_all_tasks()
{
	if(current_task->uid != 0) return;
	super_cli();
	task_t *t = (task_t *)kernel_task->next;
	while(t)
	{
		if(t != current_task && !(t->flags & TF_KTASK))
			t->state = TASK_DEAD;
		t = t->next;
	}
	super_sti();
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

int get_mem_usage()
{
	unsigned count=0;
	unsigned int*pd = (unsigned *)current_task->pd;
	int D = PAGE_DIR_IDX(TOP_TASK_MEM/PAGE_SIZE);
	int i=0;
	for(i=0;i<1022;++i)
	{
		if(i<id_tables || i >= D || !pd[i])
			continue;
		++count;
		unsigned virt = i*1024*PAGE_SIZE;
		int j;
		int start=0;
		int end=1024;
		virt += PAGE_SIZE * start;
		for(j=start;j<end;++j)
		{
			if(page_tables[(virt&PAGE_MASK)/PAGE_SIZE])
				count++;
			virt += PAGE_SIZE;
		}
	}
	if(!count)
		count++;
	return count+1;
}

int get_task_mem_usage(task_t *t)
{
	if(!t || !get_task_pid(t->pid))
		return 0;
	t->mem_usage_calc=0;
	while(!t->mem_usage_calc) {
		t->flags |= TF_REQMEM;
		force_schedule();
		if(!get_task_pid(t->pid))
			return 0;
	}
	return t->mem_usage_calc;
}
