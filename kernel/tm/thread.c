#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
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
	return 0;
}

void tm_thread_kill(struct thread *thr)
{
	thr->signal = SIGKILL;
	tm_thread_raise_flag(thr, TF_SCHED);
}

