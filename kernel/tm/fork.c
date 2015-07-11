#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/map.h>
#include <sea/fs/inode.h>
#include <sea/fs/file.h>
#include <sea/cpu/interrupt.h>

static pid_t __next_pid = 0;
static pid_t __next_tid = 0;

pid_t tm_thread_next_tid(void)
{
	return add_atomic(&__next_tid, 1);
}

pid_t tm_process_next_pid(void)
{
	return add_atomic(&__next_pid, 1);
}

struct thread *tm_thread_fork(int flags)
{
	struct thread *thr = kmalloc(sizeof(struct thread));
	thr->magic = THREAD_MAGIC;
	thr->tid = tm_thread_next_tid();
	thr->flags = TF_FORK;
	thr->priority = current_thread->priority;
	thr->kernel_stack = kmalloc_a(0x1000);
	thr->sig_mask = current_thread->sig_mask;
	thr->refs = 1;
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

static struct process *tm_process_copy(int flags)
{
	/* copies the current_process structure into
	 * a new one (cloning the things that need
	 * to be cloned) */
	struct process *newp = kmalloc(sizeof(struct process));
	newp->magic = PROCESS_MAGIC;
	mm_vm_clone(&current_process->vmm_context, &newp->vmm_context);
	newp->pid = tm_process_next_pid();
	newp->cmask = current_process->cmask;
	newp->refs = 1;
	newp->tty = current_process->tty;
	newp->heap_start = current_process->heap_start;
	newp->heap_end = current_process->heap_end;
	newp->global_sig_mask = current_process->global_sig_mask;
	memcpy((void *)newp->signal_act, current_process->signal_act, sizeof(struct sigaction) * 128);
	tm_process_inc_reference(current_process);
	newp->parent = current_process;
	ll_create(&newp->threadlist);
	ll_create_lockless(&newp->mappings);
	mutex_create(&newp->map_lock, 0);
	mutex_create(&newp->stacks_lock, 0);
	/* TODO: what the fuck is this? */
	valloc_create(&newp->mmf_valloc, MMF_BEGIN, MMF_END, PAGE_SIZE, VALLOC_USERMAP);
	for(addr_t a = MMF_BEGIN;a < (MMF_BEGIN + (size_t)newp->mmf_valloc.nindex);a+=PAGE_SIZE)
		mm_vm_set_attrib(a, PAGE_PRESENT | PAGE_WRITE);
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
	mutex_create(&newp->files_lock, 0);
	mutex_create(&newp->map_lock, 0);
	return newp;
}

void tm_thread_add_to_process(struct thread *thr, struct process *proc)
{
	ll_do_insert(&proc->threadlist, &thr->pnode, thr);
	thr->process = proc;
	tm_process_inc_reference(proc);
	add_atomic(&proc->thread_count, 1);
}

void tm_thread_add_to_cpu(struct thread *thr, struct cpu *cpu)
{
	thr->cpu = cpu;
	add_atomic(&cpu->numtasks, 1);
	tqueue_insert(cpu->active_queue, thr, &thr->activenode);
}

int sys_clone(int flags)
{
	struct process *proc = current_process;
	if(!(flags & CLONE_SHARE_PROCESS)) {
		proc = tm_process_copy(flags);
		add_atomic(&running_processes, 1);
		tm_process_inc_reference(proc);
		assert(!hash_table_set_entry(process_table, &proc->pid, sizeof(proc->pid), 1, proc));
		ll_do_insert(process_list, &proc->listnode, proc);
	}
	struct thread *thr = tm_thread_fork(flags);
	assert(!hash_table_set_entry(thread_table, &thr->tid, sizeof(thr->tid), 1, thr));
	add_atomic(&running_threads, 1);
	tm_thread_add_to_process(thr, proc);
	thr->state = THREAD_UNINTERRUPTIBLE;
	thr->usermode_stack_num = tm_thread_reserve_usermode_stack(thr);
	thr->usermode_stack_end = tm_thread_usermode_stack_end(thr->usermode_stack_num);

	cpu_disable_preemption();
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
	cpu_enable_preemption();

	if(current_thread == thr) {
		current_thread->jump_point = 0;
		cpu_enable_preemption();
		cpu_interrupt_set(1);
		return 0;
	} else {
		thr->state = THREAD_RUNNING;
		return thr->tid;
	}
	
}

int sys_vfork(void)
{

}

