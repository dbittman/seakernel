/* Forking of processes. */
#include <kernel.h>
#include <memory.h>
#include <task.h>
extern void copy_update_stack(unsigned old, unsigned new, unsigned length);

void copy_task_struct(task_t *new, task_t *parent)
{
	new->parent = parent;
	new->pid = next_pid++;
	if(parent->exe) change_icount((new->exe = parent->exe), 1);
	if(parent->root) change_icount((new->root = parent->root), 1);
	if(parent->pwd) change_icount((new->pwd = parent->pwd), 1);
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
	new->kernel_stack2 = kmalloc(KERN_STACK_SIZE+8);
	if(parent->mmf_priv_space) {
		new->mmf_priv_space = (vma_t *)kmalloc(sizeof(vma_t));
		memcpy(new->mmf_priv_space, parent->mmf_priv_space, sizeof(vma_t));
	}
	new->mmf_share_space = parent->mmf_share_space;
	copy_mmf(parent, new);
	memcpy((void *)new->signal_act, (void *)parent->signal_act, 128 * sizeof(struct sigaction));
	/* This actually duplicates the handles... */
	copy_file_handles(parent, new);
}

__attribute__((always_inline)) inline static int engage_new_stack(task_t *new, task_t *parent)
{
	assert(parent == current_task);
	u32int ebp;
	u32int esp;
	asm("mov %%esp, %0" : "=r"(esp));
	asm("mov %%ebp, %0" : "=r"(ebp));
	if(new->pid > 0 && DO_USER_MODE && esp > TOP_TASK_MEM) {
		new->esp=(esp-parent->kernel_stack) + new->kernel_stack;
		new->ebp=(ebp-parent->kernel_stack) + new->kernel_stack;
		copy_update_stack(new->kernel_stack, parent->kernel_stack, KERN_STACK_SIZE);
		return 1;
	} else {
		new->esp=esp;
		new->ebp=ebp;
		return 0;
	}
}

__attribute__((always_inline)) inline static void add_task(task_t *new)
{
	asm("cli");
	task_t *t = kernel_task->next;
	if(t)
		t->prev = new;
	new->prev = kernel_task;
	new->next = t;
	kernel_task->next = new;
}

int fork()
{
	if(!current_task || !kernel_task)
		panic(PANIC_NOSYNC, "fork() called before tasking can work!");
	assert(count_tasks() < MAX_TASKS || MAX_TASKS == -1);
	unsigned eip;
	flush_pd();
	task_t *new = (task_t *)kmalloc(sizeof(task_t));
	page_dir_t *newspace = vm_clone(current_task->pd, 0);
	if(!newspace)
	{
		kfree((void *)new);
		return -ENOMEM;
	}
	/* Create the new task structure */
	task_t *parent = (task_t *)current_task;
	new->pd = newspace;
	copy_task_struct(new, parent);
	int mp = new->pid;
	lock_scheduler();
	/* Copy the stack */
	__super_cli();
	engage_new_stack(new, parent);
	/* Set the state as frozen temporarily, so that it doesn't accidentally run it.
	 * And then add it to the queue */
	new->state = TASK_FROZEN;
	add_task(new);
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
		unlock_scheduler();
		return new->pid;
	} else
		return 0;
	return -1;
}
