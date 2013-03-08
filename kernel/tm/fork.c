/* Forking of processes. */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <atomic.h>
#include <cpu.h>

extern void copy_update_stack(unsigned old, unsigned new, unsigned length);

void copy_task_struct(task_t *new, task_t *parent)
{
	new->parent = parent;
	new->pid = add_atomic(&next_pid, 1)-1;
	if(parent->root) {
		new->root = parent->root;
		add_atomic(&new->root->count, 1);
	}
	if(parent->pwd) {
		new->pwd = parent->pwd;
		add_atomic(&new->pwd->count, 1);
	}
	new->uid = parent->uid;
	new->magic = TASK_MAGIC;
	new->gid = parent->gid;
	new->_uid = parent->_uid;
	new->_gid = parent->_gid;
	new->tty = parent->tty;
	new->sig_mask = parent->sig_mask;
	new->global_sig_mask = parent->global_sig_mask;
	new->priority = parent->priority;
	new->stack_end = parent->stack_end;
	new->heap_end = parent->heap_end;
	new->heap_start = parent->heap_start;
	new->system = parent->system;
	new->cmask = parent->cmask;
	new->path_loc_start = parent->path_loc_start;
	new->kernel_stack = kmalloc(KERN_STACK_SIZE+8);
	if(parent->mmf_priv_space) {
		new->mmf_priv_space = (vma_t *)kmalloc(sizeof(vma_t));
		memcpy(new->mmf_priv_space, parent->mmf_priv_space, sizeof(vma_t));
	}
	new->mmf_share_space = parent->mmf_share_space;
	copy_mmf(parent, new);
	memcpy((void *)new->signal_act, (void *)parent->signal_act, 128 * 
		sizeof(struct sigaction));
	/* This actually duplicates the handles... */
	copy_file_handles(parent, new);
	new->flags = TF_FORK;
	new->phys_mem_usage = parent->phys_mem_usage;
}

__attribute__((always_inline)) 
inline static int engage_new_stack(task_t *new, task_t *parent)
{
	assert(parent == current_task);
	u32int ebp;
	u32int esp;
	asm("mov %%esp, %0" : "=r"(esp));
	asm("mov %%ebp, %0" : "=r"(ebp));
	if(new->pid > 0 && esp > TOP_TASK_MEM) {
		new->esp=(esp-parent->kernel_stack) + new->kernel_stack;
		new->ebp=(ebp-parent->kernel_stack) + new->kernel_stack;
		new->sysregs = (parent->sysregs - parent->kernel_stack) + new->kernel_stack;
		copy_update_stack(new->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		new->sysregs = parent->sysregs;
		new->esp=esp;
		new->ebp=ebp;
		return 0;
	}
}

int do_fork(unsigned flags)
{
	if(!current_task || !kernel_task)
		panic(PANIC_NOSYNC, "fork() called before tasking can work!");
	/* TODO */
	//assert(count_tasks() < MAX_TASKS || MAX_TASKS == -1);
	unsigned eip;
	flush_pd();
	task_t *new = (task_t *)kmalloc(sizeof(task_t));
	page_dir_t *newspace;
	if(flags & FORK_SHAREDIR)
		newspace = vm_copy(current_task->pd);
	else
		newspace = vm_clone(current_task->pd, 0);
	if(!newspace)
	{
		kfree((void *)new);
		return -ENOMEM;
	}
	/* set the address space's entry for the current task.
	 * this is a fast and easy way to store the "what task am I" data
	 * that gets automatically updated when the scheduler switches
	 * into a new address space */
	newspace[PAGE_DIR_IDX(SMP_CUR_TASK/PAGE_SIZE)] = (unsigned)new;
	/* Create the new task structure */
	task_t *parent = (task_t *)current_task;
	new->pd = newspace;
	copy_task_struct(new, parent);
	/* Set the state as usleep temporarily, so that it doesn't accidentally run.
	 * And then add it to the queue */
	new->state = TASK_USLEEP;
	new->listnode = tqueue_insert(primary_queue, (void *)new);
	new->activenode = tqueue_insert(primary_cpu.active_queue, (void *)new);
	new->cpu = &primary_cpu;
	/* Copy the stack */
	cli();
	engage_new_stack(new, parent);
	/* Here we read the EIP of this exact location. The parent then sets the
	 * eip of the child to this. On the reschedule for the child, it will 
	 * start here as well. */
	eip = read_eip();
	if((task_t *)current_task == parent)
	{
		/* These last two fields allow full execution of the task */
		new->eip=eip;
		new->state = TASK_RUNNING;
		/* And unlock everything and reschedule */
		__engage_idle();
		return new->pid;
	}
	return 0;
}
