#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
size_t running_threads = 0;
struct hash_table *thread_table;
void tm_thread_enter_system(int sys)
{
	current_thread->system=(!sys ? -1 : sys);
}

/* TODO: add (void) to all non-argument taking functions */
void tm_thread_exit_system()
{
	current_thread->system=0;
}

int tm_thread_runnable(struct thread *thr)
{
	if(thr->state == THREAD_RUNNING)
		return 1;
	if(thr->state == THREAD_INTERRUPTIBLE && tm_thread_got_signal(thr))
		return 1;
	return 0;
}

/* TODO: remove this function? */
void tm_thread_kill(struct thread *thr)
{
	tm_signal_send_thread(thr, SIGKILL);
}

struct process *tm_thread_get(pid_t tid)
{
	struct thread *thr;
	/* TODO: should we reference count thread structures? */
	if(hash_table_get_entry(thread_table, &tid, sizeof(tid), 1, (void **)&thr) == -ENOENT)
		return 0;
	return thr;
}

