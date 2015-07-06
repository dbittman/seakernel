#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
void tm_thread_destroy(struct thread *thr)
{
	

}

void tm_process_destroy(struct process *proc)
{
	
}

void tm_process_exit(int code)
{
	//close_the_files();

	//workqueue_insert(__FREE_PROCESS_DATA);
}

void tm_thread_exit(int code)
{
	assert(current_thread->blocklist == 0);

	cpu_disable_preemption();
	ll_do_remove(&current_process->threadlist, &current_thread->pnode, 0);
	/* TODO: remove from global hash table */
	if(sub_atomic(&current_process->thread_count, 1) == 0)
		tm_process_exit(code);
	//workqueue_insert(__FREE_THREAD_DATA);
}

void sys_exit(int code)
{
	tm_thread_exit(code);
	panic(0, "tried to return from exit");
}

