/* Forking of processes. */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <atomic.h>
#include <cpu.h>

unsigned running_processes = 0;

void copy_task_struct(task_t *task, task_t *parent, char share_thread_data)
{
	task->parent = parent;
	task->pid = add_atomic(&next_pid, 1)-1;
	/* copy over the data if we're a new process. If this is going to be a thread, 
	 * then add to the count and set the pointer */
	if(!share_thread_data) {
		task->thread = (void *)kmalloc(sizeof(struct thread_shared_data));
		task->thread->count = 1;
		if(parent->thread->root) {
			task->thread->root = parent->thread->root;
			add_atomic(&task->thread->root->count, 1);
		}
		if(parent->thread->pwd) {
			task->thread->pwd = parent->thread->pwd;
			add_atomic(&task->thread->pwd->count, 1);
		}
		memcpy((void *)task->thread->signal_act, (void *)parent->thread->signal_act, 128 * 
			sizeof(struct sigaction));
		task->thread->gid = parent->thread->gid;
		task->thread->uid = parent->thread->uid;
		task->thread->_uid = parent->thread->_uid;
		task->thread->_gid = parent->thread->_gid;
		mutex_create(&(task->thread->files_lock), 0);
		task->thread->global_sig_mask = parent->thread->global_sig_mask;
	} else {
		add_atomic(&parent->thread->count, 1);
		task->thread = parent->thread;
	}
	task->magic = TASK_MAGIC;
	task->tty = parent->tty;
	task->sig_mask = parent->sig_mask;
	
	task->priority = parent->priority;
	task->stack_end = parent->stack_end;
	task->heap_end = parent->heap_end;
	task->heap_start = parent->heap_start;
	task->system = parent->system;
	task->cmask = parent->cmask;
	task->path_loc_start = parent->path_loc_start;
	task->kernel_stack = kmalloc(KERN_STACK_SIZE+8);
	if(parent->mmf_priv_space) {
		task->mmf_priv_space = (vma_t *)kmalloc(sizeof(vma_t));
		memcpy(task->mmf_priv_space, parent->mmf_priv_space, sizeof(vma_t));
	}
	task->mmf_share_space = parent->mmf_share_space;
	copy_mmf(parent, task);
	
	/* This actually duplicates the handles... */
	copy_file_handles(parent, task);
	task->flags = TF_FORK;
	mutex_create((mutex_t *)&task->exlock, MT_NOSCHED);
	task->phys_mem_usage = parent->phys_mem_usage;
	task->listnode = (void *)kmalloc(sizeof(struct llistnode));
	task->activenode = (void *)kmalloc(sizeof(struct llistnode));
	task->blocknode = (void *)kmalloc(sizeof(struct llistnode));
}

__attribute__((always_inline)) 
inline static int engage_new_stack(task_t *task, task_t *parent)
{
	assert(parent == current_task);
	u32int ebp;
	u32int esp;
	asm("mov %%esp, %0" : "=r"(esp));
	asm("mov %%ebp, %0" : "=r"(ebp));
	if(esp > TOP_TASK_MEM) {
		task->esp=(esp-parent->kernel_stack) + task->kernel_stack;
		task->ebp=(ebp-parent->kernel_stack) + task->kernel_stack;
		task->sysregs = (parent->sysregs - parent->kernel_stack) + task->kernel_stack;
		copy_update_stack(task->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		task->sysregs = parent->sysregs;
		task->esp=esp;
		task->ebp=ebp;
		return 0;
	}
}

#if CONFIG_SMP
unsigned int __counter = 0;
cpu_t *fork_choose_cpu(task_t *parent)
{
	cpu_t *pc = parent->cpu;
	cpu_t *cpu = &cpu_array[__counter];
	add_atomic(&__counter, 1);
	if(__counter >= num_cpus)
		__counter=0;
	if(!(cpu->flags & CPU_TASK))
		return pc;
	if(cpu->active_queue->num < 2) return cpu;
	for(unsigned int i=0;i<num_cpus;i++) {
		cpu_t *tmp = &cpu_array[i];
		if(tmp->active_queue->num < cpu->active_queue->num)
			cpu = tmp;
	}
	return cpu;
}
#endif

int do_fork(unsigned flags)
{
	assert(current_task && kernel_task);
	assert(running_processes < (unsigned)MAX_TASKS || MAX_TASKS == -1);
	unsigned eip;
	flush_pd();
	task_t *task = (task_t *)kmalloc(sizeof(task_t));
	page_dir_t *newspace;
	if(flags & FORK_SHAREDIR)
		newspace = vm_copy(current_task->pd);
	else
		newspace = vm_clone(current_task->pd, 0);
	if(!newspace)
	{
		kfree((void *)task);
		return -ENOMEM;
	}
	/* set the address space's entry for the current task.
	 * this is a fast and easy way to store the "what task am I" data
	 * that gets automatically updated when the scheduler switches
	 * into a new address space */
	newspace[PAGE_DIR_IDX(SMP_CUR_TASK/PAGE_SIZE)] = (unsigned)task;
	/* Create the new task structure */
	task_t *parent = (task_t *)current_task;
	task->pd = newspace;
	copy_task_struct(task, parent, flags & FORK_SHAREDAT);
	add_atomic(&running_processes, 1);
	/* Set the state as usleep temporarily, so that it doesn't accidentally run.
	 * And then add it to the queue */
	task->state = TASK_USLEEP;
	tqueue_insert(primary_queue, (void *)task, task->listnode);
	cpu_t *cpu = (cpu_t *)parent->cpu;
#if CONFIG_SMP
	cpu = fork_choose_cpu(parent);
#endif
	/* Copy the stack */
	set_int(0);
	engage_new_stack(task, parent);
	/* Here we read the EIP of this exact location. The parent then sets the
	 * eip of the child to this. On the reschedule for the child, it will 
	 * start here as well. */
	eip = read_eip();
	if((task_t *)current_task == parent)
	{
		/* These last things allow full execution of the task */
		task->eip=eip;
		task->state = TASK_RUNNING;
		mutex_acquire(&cpu->lock);
		task->cpu = cpu;
		add_atomic(&cpu->numtasks, 1);
		tqueue_insert(cpu->active_queue, (void *)task, task->activenode);
		mutex_release(&cpu->lock);
		/* And unlock everything and reschedule */
		__engage_idle();
		return task->pid;
	}
	return 0;
}
