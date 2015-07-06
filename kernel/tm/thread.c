#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
size_t running_threads = 0;
struct hash_table *thread_table;
void tm_thread_enter_system(int sys)
{
	current_thread->system=(!sys ? -1 : sys);
}

void tm_thread_exit_system()
{
	current_thread->system=0;
}

