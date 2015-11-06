#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/lib/linkedlist.h>
#include <sea/tm/kthread.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/map.h>
#include <sea/fs/inode.h>
#include <sea/fs/file.h>
#include <sea/vsprintf.h>
#include <sea/fs/kerfs.h>
#include <sea/fs/dir.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>

static pid_t __next_pid = 0;
static pid_t __next_tid = 0;

pid_t tm_thread_next_tid(void)
{
	return atomic_fetch_add(&__next_tid, 1) + 1;
}

pid_t tm_process_next_pid(void)
{
	return atomic_fetch_add(&__next_pid, 1) + 1;
}

extern struct filesystem *devfs;
#define __expose_proc_field(proc, field, type) \
	do { \
		char file[128];\
		snprintf(file, 128, "/dev/process/%d/%s", proc->pid, #field); \
		kerfs_register_parameter(file, (void *)&proc->field, sizeof(proc->field), \
				0, type); \
	} while(0)
#define __expose_thread_field(thr, field, type) \
	do { \
		char file[128];\
		snprintf(file, 128, "/dev/process/%d/%d/%s", proc->pid, thr->tid, #field); \
		kerfs_register_parameter(file, &thr->field, sizeof(thr->field), \
				0, type); \
	} while(0)

int kerfs_mappings_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	struct process *proc = param;
	assert(proc->magic == PROCESS_MAGIC);
	KERFS_PRINTF(offset, length, buf, current,
				"           START              END   LENGTH FLAGS    INODE   OFFSET\n");
	mutex_acquire(&proc->map_lock);
	
	struct linkedentry *node;
	struct memmap *map;
	for(node = linkedlist_iter_start(&proc->mappings);
			node != linkedlist_iter_end(&proc->mappings);
			node = linkedlist_iter_next(node)) {
		map = linkedentry_obj(node);
		char flags[6];
		memset(flags, ' ', sizeof(flags));
		if(map->flags & MAP_SHARED)
			flags[0] = 'S';
		if(map->flags & MAP_FIXED)
			flags[1] = 'F';
		if(map->flags & MAP_ANONYMOUS)
			flags[2] = 'A';
		if(map->prot & PROT_WRITE)
			flags[3] = 'W';
		if(map->prot & PROT_EXEC)
			flags[4] = 'E';
		flags[5] = 0;
		KERFS_PRINTF(offset, length, buf, current,
				"%16x %16x %8x %s %8d %8x\n",
				map->virtual, map->virtual + map->length, map->length,
				flags, map->node->id, map->offset);
	}

	mutex_release(&proc->map_lock);
	
	return current;
}

void tm_process_create_kerfs_entries(struct process *proc)
{
	if(!devfs)
		return;
	char path[32];
	snprintf(path, 32, "/dev/process/%d", proc->pid);
	int ret = sys_mkdir(path, 0755);
	if(ret < 0) {
		printk(2, "[tm]: failed to create process entry %d: err=%d\n", proc->pid, -ret);
		return;
	}
	__expose_proc_field(proc, heap_start, kerfs_rw_address);
	__expose_proc_field(proc, flags, kerfs_rw_integer);
	__expose_proc_field(proc, refs, kerfs_rw_integer);
	__expose_proc_field(proc, heap_end, kerfs_rw_address);
	__expose_proc_field(proc, cmask, kerfs_rw_integer);
	__expose_proc_field(proc, effective_uid, kerfs_rw_integer);
	__expose_proc_field(proc, effective_gid, kerfs_rw_integer);
	__expose_proc_field(proc, real_uid, kerfs_rw_integer);
	__expose_proc_field(proc, real_gid, kerfs_rw_integer);
	__expose_proc_field(proc, tty, kerfs_rw_integer);
	__expose_proc_field(proc, utime, kerfs_rw_integer);
	__expose_proc_field(proc, stime, kerfs_rw_integer);
	__expose_proc_field(proc, thread_count, kerfs_rw_integer);
	__expose_proc_field(proc, global_sig_mask, kerfs_rw_integer);
	__expose_proc_field(proc, command, kerfs_rw_string);
	__expose_proc_field(proc, exit_reason.sig, kerfs_rw_integer);
	__expose_proc_field(proc, exit_reason.pid, kerfs_rw_integer);
	__expose_proc_field(proc, exit_reason.ret, kerfs_rw_integer);
	__expose_proc_field(proc, exit_reason.coredump, kerfs_rw_integer);
	__expose_proc_field(proc, exit_reason.cause, kerfs_rw_integer);
	char file[128];
	snprintf(file, 128, "/dev/process/%d/maps", proc->pid);
	kerfs_register_parameter(file, proc, sizeof(void *), 0, kerfs_mappings_report);
}

void tm_thread_create_kerfs_entries(struct thread *thr)
{
	struct process *proc = thr->process;
	if(!devfs)
		return;
	char path[48];
	snprintf(path, 48, "/dev/process/%d/%d", proc->pid, thr->tid);
	int ret = sys_mkdir(path, 0755);
	if(ret < 0) {
		printk(2, "[tm]: failed to create thread entry %d: err=%d\n", thr->tid, -ret);
		return;
	}
	__expose_thread_field(thr, refs, kerfs_rw_integer);
	__expose_thread_field(thr, state, kerfs_rw_integer);
	__expose_thread_field(thr, flags, kerfs_rw_address);
	__expose_thread_field(thr, system, kerfs_rw_integer);
	__expose_thread_field(thr, priority, kerfs_rw_integer);
	__expose_thread_field(thr, timeslice, kerfs_rw_integer);
	__expose_thread_field(thr, usermode_stack_end, kerfs_rw_address);
	__expose_thread_field(thr, sig_mask, kerfs_rw_address);
	__expose_thread_field(thr, cpuid, kerfs_rw_address);
	__expose_thread_field(thr, blocklist, kerfs_rw_address);
}

struct thread *tm_thread_fork(int flags)
{
	struct thread *thr = kmalloc(sizeof(struct thread));
	thr->magic = THREAD_MAGIC;
	thr->tid = tm_thread_next_tid();
	thr->priority = current_thread->priority;
	thr->sig_mask = current_thread->sig_mask;
	thr->refs = 1;
	spinlock_create(&thr->status_lock);
	workqueue_create(&thr->resume_work, 0);
	return thr;
}

static void __copy_mappings(struct process *ch, struct process *pa)
{
	/* copy the memory mappings. All mapping types are inherited, but
	 * they only barely modified. Like, just enough for the new process
	 * to not screw up the universe. This is accomplished by the fact
	 * that everything is just pointers to sections of memory that are
	 * copied during mm_vm_clone anyway... */
	//mutex_acquire(&pa->map_lock);
	struct linkedentry *node;
	struct memmap *map;
	if(pa != kernel_process) {
		for(node = linkedlist_iter_start(&pa->mappings);
				node != linkedlist_iter_end(&pa->mappings);
				node = linkedlist_iter_next(node)) {
			map = linkedentry_obj(node);
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
			linkedlist_insert(&ch->mappings, &n->entry, n);
		}
	}
	/* for this simple copying of data to work, we rely on the address space being cloned BEFORE
	 * this is called, so the pointers are actually valid */
	memcpy(&(ch->mmf_valloc), &(pa->mmf_valloc), sizeof(pa->mmf_valloc));
	mutex_create(&ch->mmf_valloc.lock, 0);
	//mutex_release(&pa->map_lock);
}

static struct process *tm_process_copy(int flags, struct thread *newthread)
{
	/* copies the current_process structure into
	 * a new one (cloning the things that need
	 * to be cloned) */
	struct process *newp = kmalloc(sizeof(struct process));
	newp->magic = PROCESS_MAGIC;
	mm_context_clone(&current_process->vmm_context, &newp->vmm_context);
	newp->pid = tm_process_next_pid();
	newp->cmask = current_process->cmask;
	newp->refs = 1;
	newp->tty = current_process->tty;
	newp->heap_start = current_process->heap_start;
	newp->heap_end = current_process->heap_end;
	newp->global_sig_mask = current_process->global_sig_mask;
	memcpy((void *)newp->signal_act, current_process->signal_act, sizeof(struct sigaction) * NUM_SIGNALS);
	tm_process_inc_reference(current_process);
	newp->parent = current_process;
	linkedlist_create(&newp->threadlist, 0);
	blocklist_create(&newp->waitlist, 0, "process-waitlist");
	linkedlist_create(&newp->mappings, LINKEDLIST_LOCKLESS);
	mutex_create(&newp->map_lock, 0); /* we need to lock this during page faults */
	mutex_create(&newp->stacks_lock, 0);
	valloc_create(&newp->mmf_valloc, MEMMAP_MMAP_BEGIN, MEMMAP_MMAP_END, PAGE_SIZE, 0);
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
	tm_process_create_kerfs_entries(newp);
	return newp;
}

void tm_thread_add_to_process(struct thread *thr, struct process *proc)
{
	linkedlist_insert(&proc->threadlist, &thr->pnode, thr);
	thr->process = proc;
	tm_process_inc_reference(proc);
	atomic_fetch_add(&proc->thread_count, 1);
	tm_thread_create_kerfs_entries(thr);
}

void tm_thread_add_to_cpu(struct thread *thr, struct cpu *cpu)
{
	thr->cpu = cpu;
	thr->cpuid = cpu->knum;
	atomic_fetch_add(&cpu->numtasks, 1);
	tqueue_insert(cpu->active_queue, thr, &thr->activenode);
}

struct cpu *tm_fork_pick_cpu(void)
{
#if CONFIG_SMP
	static int last_cpu = 0;
	struct cpu *cpu = cpu_get(atomic_fetch_add(&last_cpu, 1)+1);
	if(!(cpu->flags & CPU_RUNNING))
		cpu = 0;
	if(!cpu) {
		last_cpu = 0;
		return primary_cpu;
	}
	return cpu;
#else
	return primary_cpu;
#endif
}

__attribute__((optimize("-O0"))) __attribute__((noinline)) static struct thread *__post_fork_get_current()
{
	return current_thread;
}

void arch_tm_fork_init(struct thread *thread);
void arch_tm_userspace_fork_syscall_return(void);
int tm_clone(int flags, void *entry, struct kthread *kt)
{
	struct process *proc = current_process;
	struct thread *thr = tm_thread_fork(flags);
	if(!(flags & CLONE_SHARE_PROCESS) && !(flags & CLONE_KTHREAD)) {
		proc = tm_process_copy(flags, thr);
		atomic_fetch_add_explicit(&running_processes, 1, memory_order_relaxed);
		tm_process_inc_reference(proc);
		hash_insert(process_table, &proc->pid, sizeof(proc->pid), &proc->hash_elem, proc);
		linkedlist_insert(process_list, &proc->listnode, proc);
	} else if(flags & CLONE_KTHREAD) {
		proc = kernel_process;
	}
	hash_insert(thread_table, &thr->tid, sizeof(thr->tid), &thr->hash_elem, thr);
	atomic_fetch_add_explicit(&running_threads, 1, memory_order_relaxed);
	tm_thread_add_to_process(thr, proc);
	if(!tm_thread_reserve_stacks(thr))
		panic(0, "NOT IMPLEMENTED: NO MORE STACKS");
	if(proc == current_process && proc != kernel_process) {
		addr_t ret = mm_mmap(thr->usermode_stack_start, CONFIG_STACK_PAGES * PAGE_SIZE,
				PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	}
	size_t kms_page_size = mm_page_size_closest(KERN_STACK_SIZE);
	for(unsigned int i = 0;i<((KERN_STACK_SIZE-1) / kms_page_size)+1;i++) {
		addr_t phys = mm_physical_allocate(kms_page_size, false);
		bool r = mm_context_virtual_map(&proc->vmm_context, thr->kernel_stack + i * kms_page_size,
				phys, PAGE_PRESENT | PAGE_WRITE, kms_page_size);
		if(!r)
			mm_physical_deallocate(phys);
	}
	if(current_thread->regs) {
		arch_tm_fork_init(thr);
	} else {
		thr->stack_pointer = thr->kernel_stack + KERN_STACK_SIZE;
	}
	bool r = mm_context_write(&proc->vmm_context, thr->kernel_stack, &thr, sizeof(&thr));
	assert(r);

	if(flags & CLONE_KTHREAD) {
		kt->thread = thr;
		thr->kernel_thread = kt;
	}
	thr->state = THREADSTATE_UNINTERRUPTIBLE;

	struct cpu *target_cpu = tm_fork_pick_cpu();

	cpu_disable_preemption();
	int old = cpu_interrupt_set(0);
	thr->jump_point = (addr_t)entry;
	thr->state = THREADSTATE_RUNNING;
	tm_thread_add_to_cpu(thr, target_cpu);
	cpu_interrupt_set(old);
	cpu_enable_preemption();
	if(flags & CLONE_SHARE_PROCESS)
		return thr->tid;
	else
		return proc->pid;
}

int sys_clone(int f)
{
	/* userspace calls aren't allowed to fork a kernel thread. */
	f &= ~CLONE_KTHREAD;
	return tm_clone(f, arch_tm_userspace_fork_syscall_return, NULL);
}

/* vfork is a performance hack designed to make the normal fork-exec proces
 * faster by not copying page tables during fork. There isn't really a reason
 * for this now, since we can just do copy-on-write. So....just have this
 * call normal fork. */
int sys_vfork(void)
{
	return sys_clone(0);
}

