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
	add_atomic(&proc->refs, 1);
	mutex_release(&process_refs_lock);
	return proc;
}

void tm_process_inc_reference(struct process *proc)
{
	if(!(proc->refs >= 1))
		panic(PANIC_NOSYNC, "process refcount error: %d refs = %d\n", proc->pid, proc->refs);
	add_atomic(&proc->refs, 1);
}

void tm_process_put(struct process *proc)
{
	mutex_acquire(&process_refs_lock);
	if(!(proc->refs >= 1))
		panic(PANIC_NOSYNC, "process refcount error: %d refs = %d\n", proc->pid, proc->refs);
	if(sub_atomic(&proc->refs, 1) == 0) {
		hash_table_delete_entry(process_table, &proc->pid, sizeof(proc->pid), 1);
		mutex_release(&process_refs_lock);
		kfree(proc);
	} else {
		mutex_release(&process_refs_lock);
	}
}

