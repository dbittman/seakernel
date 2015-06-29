#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/schedule.h>
void tm_process_enter_system(int sys)
{
	current_task->system=(!sys ? -1 : sys);
	current_task->cur_ts/=2;
}

void tm_process_exit_system()
{
	current_task->last = current_task->system;
	current_task->system=0;
}

