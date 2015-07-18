#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/lib/bitmap.h>
#include <sea/mm/valloc.h>
size_t running_threads = 0;
struct hash_table *thread_table;
mutex_t thread_refs_lock;
struct valloc km_stacks;
void tm_thread_enter_system(int sys)
{
	current_thread->system=(!sys ? -1 : sys);
}

void tm_thread_exit_system(void)
{
	current_thread->system=0;
}

int tm_thread_runnable(struct thread *thr)
{
	if(thr->state == THREADSTATE_RUNNING)
		return 1;
	if(thr->state == THREADSTATE_INTERRUPTIBLE && tm_thread_got_signal(thr))
		return 1;
	return 0;
}

struct thread *tm_thread_get(pid_t tid)
{
	struct thread *thr;
	mutex_acquire(&thread_refs_lock);
	if(hash_table_get_entry(thread_table, &tid, sizeof(tid), 1, (void **)&thr) == -ENOENT) {
		mutex_release(&thread_refs_lock);
		return 0;
	}
	add_atomic(&thr->refs, 1);
	mutex_release(&thread_refs_lock);
	return thr;
}

void tm_thread_inc_reference(struct thread *thr)
{
	assert(thr->refs >= 1);
	add_atomic(&thr->refs, 1);
}

void tm_thread_put(struct thread *thr)
{
	assert(thr->refs >= 1);
	mutex_acquire(&thread_refs_lock);
	if(sub_atomic(&thr->refs, 1) == 0) {
		hash_table_delete_entry(thread_table, &current_thread->tid, sizeof(current_thread->tid), 1);
		mutex_release(&thread_refs_lock);
		kfree(thr);
	} else {
		mutex_release(&thread_refs_lock);
	}
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
	return (stack + 1) * (CONFIG_STACK_PAGES * PAGE_SIZE) + USERMODE_STACKS_START;
}

addr_t tm_thread_reserve_kernelmode_stack(void)
{
	struct valloc_region va;
	if(!valloc_allocate(&km_stacks, &va, 1))
		panic(PANIC_NOSYNC, "unable to allocate kernel mode stack");
	for(addr_t a = va.start; a < va.start + KERN_STACK_SIZE; a += PAGE_SIZE)
		map_if_not_mapped(a);
	return va.start;
}

void tm_thread_release_kernelmode_stack(addr_t base)
{
	struct valloc_region va;
	va.start = base;
	va.npages = 1;
	va.flags = 0;
	valloc_deallocate(&km_stacks, &va);
}

