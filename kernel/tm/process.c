#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/ll.h>
struct hash_table *process_table;
struct llist *process_list;
size_t running_processes = 0;

struct process *tm_process_get(pid_t pid)
{
	struct process *proc;
	/* TODO: should we reference count process structures? */
	if(hash_table_get_entry(process_table, &pid, sizeof(pid), 1, (void **)&proc) == -ENOENT)
		return 0;
	return proc;
}
