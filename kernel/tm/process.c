#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/schedule.h>
#include <sea/lib/hash.h>
struct hash_table *process_table;

void tm_thread_enter_system(int sys)
{
	current_thread->system=(!sys ? -1 : sys);
	current_thread->cur_ts/=2;
}

void tm_thread_exit_system()
{
	current_thread->last = current_task->system;
	current_thread->system=0;
}

