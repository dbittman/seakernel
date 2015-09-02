#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>
struct hash_table *process_table;
struct llist *process_list;
size_t running_processes = 0;

mutex_t process_refs_lock;

struct process *tm_process_get(pid_t pid)
{
	struct process *proc;
	mutex_acquire(&process_refs_lock);
	if(hash_table_get_entry(process_table, &pid, sizeof(pid), 1, (void **)&proc) == -ENOENT) {
		mutex_release(&process_refs_lock);
		return 0;
	}
	atomic_fetch_add(&proc->refs, 1);
	mutex_release(&process_refs_lock);
	return proc;
}

void tm_process_inc_reference(struct process *proc)
{
	assert(proc->magic == PROCESS_MAGIC);
	if(!(proc->refs >= 1))
		panic(PANIC_NOSYNC, "process refcount error (inc): %d refs = %d\n", proc->pid, proc->refs);
	atomic_fetch_add(&proc->refs, 1);
}

void tm_process_put(struct process *proc)
{
	assert(proc->magic == PROCESS_MAGIC);
	mutex_acquire(&process_refs_lock);
	if(!(proc->refs >= 1))
		panic(PANIC_NOSYNC, "process refcount error (put): %d (%s) refs = %d\n", proc->pid, proc->command, proc->refs);
	if(atomic_fetch_sub(&proc->refs, 1) == 1) {
		hash_table_delete_entry(process_table, &proc->pid, sizeof(proc->pid), 1);
		mutex_release(&process_refs_lock);
		/* do this here...since we must wait for every thread to give up
		 * their refs. This happens in schedule, after it gets scheduled away */
		mm_destroy_directory(&proc->vmm_context);
		kfree(proc);
	} else {
		mutex_release(&process_refs_lock);
	}
}

struct thread *tm_process_get_head_thread(struct process *proc)
{
	rwlock_acquire(&proc->threadlist.rwl, RWL_READER);
	struct thread *thread = 0;
	if(proc->threadlist.num > 0) {
		thread = ll_entry(struct thread *, proc->threadlist.head);
	}
	rwlock_release(&proc->threadlist.rwl, RWL_READER);
	return thread;
}

