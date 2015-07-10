#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/lib/bitmap.h>
size_t running_threads = 0;
struct hash_table *thread_table;
void tm_thread_enter_system(int sys)
{
	current_thread->system=(!sys ? -1 : sys);
}

/* TODO: add (void) to all non-argument taking functions */
void tm_thread_exit_system(void)
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

int tm_thread_reserve_usermode_stack(struct thread *thr)
{
	unsigned i;
	mutex_acquire(&thr->process->stacks_lock);
	for(i = 0;i<NUM_USERMODE_STACKS;i++) {
		if(!bitmap_test(thr->process->stack_bitmap, i)) {
			bitmap_set(thr->process->stack_bitmap, i);
			break;
		}
	}
	mutex_release(&thr->process->stacks_lock);
	if(i == NUM_USERMODE_STACKS)
		return -ENOMEM;
	return i;
}

void tm_thread_release_usermode_stack(struct thread *thr, int stack)
{
	mutex_acquire(&thr->process->stacks_lock);
	bitmap_reset(thr->process->stack_bitmap, stack);
	mutex_release(&thr->process->stacks_lock);
}

addr_t tm_thread_usermode_stack_end(int stack)
{
	assert(stack >= 0 && (unsigned)stack < NUM_USERMODE_STACKS);
	return stack * (CONFIG_STACK_PAGES * PAGE_SIZE) + USERMODE_STACKS_START;
}

