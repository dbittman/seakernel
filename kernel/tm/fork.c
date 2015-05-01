/* Forking of processes. */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/processor.h>
#include <sea/tm/context.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/fs/inode.h>
#include <sea/tm/schedule.h>
#include <sea/ll.h>
#include <sea/mm/map.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

unsigned running_processes = 0;
static void __copy_mappings(task_t *ch, task_t *pa)
{
	/* copy the memory mappings. All mapping types are inherited, but
	 * they only barely modified. Like, just enough for the new process
	 * to not screw up the universe. This is accomplished by the fact
	 * that everything is just pointers to sections of memory that are
	 * copied during mm_vm_clone anyway... */
	mutex_acquire(&pa->thread->map_lock);
	struct llistnode *node;
	struct memmap *map;
	ll_for_each_entry(&pa->thread->mappings, node, struct memmap *, map) {
		/* new mapping object, copy the data */
		struct memmap *n = kmalloc(sizeof(*map));
		memcpy(n, map, sizeof(*n));
		/* of course, we have another reference to the backing inode */
		vfs_inode_get(n->node);
		if(map->flags & MAP_SHARED) {
			/* and if it's shared, tell the inode that another processor
			 * cares about some section of memory */
			fs_inode_map_region(n->node, n->offset, n->length);
		}
		n->entry = ll_insert(&ch->thread->mappings, n);
	}
	/* for this simple copying of data to work, we rely on the address space being cloned BEFORE
	 * this is called, so the pointers are actually valid */
	memcpy(&(ch->thread->mmf_valloc), &(pa->thread->mmf_valloc), sizeof(pa->thread->mmf_valloc));
	ch->thread->mmf_valloc.lock.lock = 0;
	mutex_release(&pa->thread->map_lock);
}

static void copy_thread_data(task_t *task, task_t *parent)
{
	/* we've created a new process, not a new thread. Copy over the data
	 * and increase reference counts */
	assert(parent->thread->magic == THREAD_MAGIC);
	if(parent->thread->root) {
		task->thread->root = parent->thread->root;
		vfs_inode_get(parent->thread->root);
	}
	if(parent->thread->pwd) {
		task->thread->pwd = parent->thread->pwd;
		vfs_inode_get(parent->thread->pwd);
	}
	memcpy((void *)task->thread->signal_act, (void *)parent->thread->signal_act, 128 * 
		sizeof(struct sigaction));
	task->thread->effective_gid = parent->thread->effective_gid;
	task->thread->effective_uid = parent->thread->effective_uid;
	task->thread->real_uid = parent->thread->real_uid;
	task->thread->real_gid = parent->thread->real_gid;
	task->thread->global_sig_mask = parent->thread->global_sig_mask;
	__copy_mappings(task, parent);
}

static void copy_task_struct(task_t *task, task_t *parent, char share_thread_data)
{
	task->parent = parent;
	task->pid = add_atomic(&next_pid, 1)-1 /* just....shut up */;
	/* copy over the data if we're a new process. If this is going to be a thread, 
	 * then add to the count and set the pointer */
	if(!share_thread_data) {
		task->thread = tm_thread_data_create();
		copy_thread_data(task, parent);
	} else {
		add_atomic(&parent->thread->count, 1);
		task->thread = parent->thread;
		assert(parent->thread->magic == THREAD_MAGIC);
	}

	task->tty = parent->tty;
	task->sig_mask = parent->sig_mask;
	task->priority = parent->priority;
	task->stack_end = parent->stack_end;
	task->heap_end = parent->heap_end;
	task->heap_start = parent->heap_start;
	task->system = parent->system;
	task->cmask = parent->cmask;
	task->path_loc_start = parent->path_loc_start;

	fs_copy_file_handles(parent, task);
	/* this flag gets cleared on the first scheduling of this task */
	task->flags = TF_FORK;
	task->phys_mem_usage = parent->phys_mem_usage;
}

#if CONFIG_SMP
extern cpu_t cpu_array[];
static int __counter = 0;
static cpu_t *fork_choose_cpu(task_t *parent)
{
	cpu_t *pc = parent->cpu;
	cpu_t *cpu = &cpu_array[__counter];
	add_atomic(&__counter, 1);
	if(__counter >= (int)num_cpus)
		__counter=0;
	if(!(cpu->flags & CPU_TASK))
		return pc;
	if(cpu->active_queue->num < 2) return cpu;
	for(int i=0;i<(int)num_cpus;i++) {
		cpu_t *tmp = &cpu_array[i];
		if(tmp->active_queue->num < cpu->active_queue->num)
			cpu = tmp;
	}
	if(!(cpu->flags & CPU_TASK))
		return pc;
	return cpu;
}
#endif

int tm_do_fork(unsigned flags)
{
	assert(current_task && kernel_task);
	assert(running_processes < (unsigned)CONFIG_MAX_TASKS || CONFIG_MAX_TASKS == -1);
	addr_t eip;
	task_t *task = tm_task_create();
	page_dir_t *newspace;
	if(flags & FORK_SHAREDIR)
		newspace = mm_vm_copy(current_task->pd);
	else
		newspace = mm_vm_clone(current_task->pd, 0);
	if(!newspace)
	{
		kfree((void *)task);
		return -ENOMEM;
	}
	/* set the address space's entry for the current task.
	 * this is a fast and easy way to store the "what task am I" data
	 * that gets automatically updated when the scheduler switches
	 * into a new address space */
	arch_tm_set_current_task_marker(newspace, (addr_t)task);
	/* Create the new task structure */
	task->pd = newspace;
	copy_task_struct(task, current_task, flags & FORK_SHAREDAT);
	add_atomic(&running_processes, 1);
	/* Set the state as usleep temporarily, so that it doesn't accidentally run.
	 * And then add it to the queue */
	task->state = TASK_USLEEP;
	tqueue_insert(primary_queue, (void *)task, task->listnode);
	cpu_t *cpu = current_task->cpu;
#if CONFIG_SMP
	cpu = fork_choose_cpu(current_task);
#endif
	/* Copy the stack */
	cpu_interrupt_set(0);
	engage_new_stack(task, current_task);
	volatile task_t *parent = current_task;
	store_context_fork(task);
	/* Here we read the EIP of this exact location. The parent then sets the
	 * eip of the child to this. On the reschedule for the child, it will 
	 * start here as well. */
	eip = tm_read_eip();
	if(current_task == parent)
	{
		/* These last things allow full execution of the task */
		task->eip=eip;
		task->state = TASK_RUNNING;
		task->cpu = cpu;
		add_atomic(&cpu->numtasks, 1);
		tqueue_insert(cpu->active_queue, (void *)task, task->activenode);
		tm_engage_idle();
		return task->pid;
	}
	return 0;
}

int sys_vfork()
{
	int pid = tm_do_fork(0);
	if(pid < 0) {
		return pid;
	} else if(pid) {
		/* parent */
		task_t *t = tm_get_process_by_pid(pid);
		/* TODO: should we clean up like wait() if the task dies? */
		while(t->state != TASK_DEAD && t->flags & TF_DIDEXEC) tm_schedule();
		return pid;
	} else {
		return 0;
	}
}

