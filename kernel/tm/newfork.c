#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/map.h>
/* TODO: hash table of threads and processes */

static pid_t __next_pid = 0;
static pid_t __next_tid = 0;

struct thread *tm_thread_fork(int flags)
{
	struct thread *thr = kmalloc(sizeof(struct thread));
	thr->magic = THREAD_MAGIC;
	thr->tid = add_atomic(&__next_tid, 1);
	thr->flags = TF_FORK;
	thr->priority = current_thread->priority;
	thr->kernel_stack = kmalloc_a(0x1000);
	thr->sig_mask = current_thread->sig_mask;
	memcpy((void *)thr->signal_act, current_thread->signal_act, sizeof(struct sigaction) * 128);
	return thr;
}

static void __copy_mappings(struct process *ch, struct process *pa)
{
	/* copy the memory mappings. All mapping types are inherited, but
	 * they only barely modified. Like, just enough for the new process
	 * to not screw up the universe. This is accomplished by the fact
	 * that everything is just pointers to sections of memory that are
	 * copied during mm_vm_clone anyway... */
	mutex_acquire(&pa->map_lock);
	struct llistnode *node;
	struct memmap *map;
	ll_for_each_entry(&pa->mappings, node, struct memmap *, map) {
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
		n->entry = ll_insert(&ch->mappings, n);
	}
	/* for this simple copying of data to work, we rely on the address space being cloned BEFORE
	 * this is called, so the pointers are actually valid */
	memcpy(&(ch->mmf_valloc), &(pa->mmf_valloc), sizeof(pa->mmf_valloc));
	ch->mmf_valloc.lock.lock = 0;
	mutex_release(&pa->map_lock);
}

struct process *tm_process_copy(int flags)
{
	/* copies the current_process structure into
	 * a new one (cloning the things that need
	 * to be cloned) */
	struct process *newp = kmalloc(sizeof(struct process));
	newp->magic = PROCESS_MAGIC;
	newp->mm_context = mm_vm_clone(current_process->mm_context, 0); /* TODO: is this the right one? */
	newp->pid = add_atomic(&__next_pid, 1);
	newp->flags = PROCESS_FORK;
	newp->cmask = current_process->cmask;
	newp->tty = current_process->tty;
	newp->heap_start = current_process->heap_start;
	newp->heap_end = current_process->heap_end;
	newp->signal_mask = current_process->signal_mask;
	newp->signal = current_process->signal; /* TODO: do we need this, or just in threads? */
	newp->parent = current_process;
	ll_create(&newp->threadlist);
	__copy_mappings(newp, current_process);
	if(current_process->root) {
		newp->root = current_process->root;
		vfs_inode_get(newp->root);
	}
	if(current_process->cwd) {
		newp->cwd = current_process->cwd;
		vfs_inode_get(newp->cwd);
	}
	fs_copy_file_handles(current_process, newp);
	return newp;
}

void tm_thread_add_to_process(struct thread *thr, struct process *proc)
{
	ll_do_insert(&proc->threadlist, &thr->pnode, thr);
	thr->process = proc;
	add_atomic(&proc->thread_count, 1);
}

void tm_thread_add_to_cpu(struct thread *thr, struct cpu *cpu)
{
	thr->cpu = cpu;
	tqueue_insert(cpu->active_queue, thr, &thr->activenode);
}

int sys_clone(int flags)
{
	struct process *proc = current_process;
	if(!(flags & CLONE_SHARE_PROCESS)) {
		proc = tm_process_copy(flags);
		add_atomic(&running_processes, 1);
		/* TODO: insert to global hash table */
		assert(!hash_table_set_entry(process_table, &proc->pid, sizeof(proc->pid), 1, proc));
	}
	struct thread *thr = tm_thread_fork(flags);
	assert(!hash_table_set_entry(thread_table, &thr->tid, sizeof(thr->tid), 1, thr));
	add_atomic(&running_threads, 1);
	tm_thread_add_to_process(thr, proc);
	thr->state = THREAD_UNINTERRUPTIBLE;
	

	arch_cpu_copy_fixup_stack((addr_t)thr->kernel_stack, (addr_t)current_thread->kernel_stack, KERN_STACK_SIZE);
	*(struct thread **)(thr->kernel_stack) = thr;
	addr_t esp;
	addr_t ebp;
	asm("mov %%esp, %0":"=r"(esp));
	asm("mov %%ebp, %0":"=r"(ebp));


	esp -= (addr_t)current_thread->kernel_stack;
	ebp -= (addr_t)current_thread->kernel_stack;
	esp += (addr_t)thr->kernel_stack;
	ebp += (addr_t)thr->kernel_stack;

	esp += 4;
	*(addr_t *)esp = ebp;
	thr->stack_pointer = esp;
	tm_thread_add_to_cpu(thr, current_thread->cpu);
	thr->jump_point = (addr_t)arch_tm_read_ip();

	if(current_thread == thr) {
		current_thread->jump_point = 0;
		cpu_interrupt_set(1);
		kprintf("CHILD %x %x %x\n", current_thread->flags, current_thread->magic, current_thread->tid);
		return 0;
	} else {
		kprintf("PARENT\n");
		thr->state = THREAD_RUNNING;
		return thr->tid;
	}
	
}

int sys_vfork()
{

}

