#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/kthread.h>
#include <sea/tm/timing.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/asm/system.h>
#include <sea/syscall.h>
#include <sea/ll.h>
#include <sea/kernel.h>
#include <sea/vsprintf.h>
#include <sea/tm/thread.h>

extern mutex_t process_refs_lock;
extern mutex_t thread_refs_lock;
extern int initial_kernel_stack;
struct process *kernel_process = 0;
void tm_init_multitasking(void)
{
	printk(KERN_DEBUG, "[sched]: Starting multitasking system...\n");
	
	process_table = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(process_table, HASH_RESIZE_MODE_IGNORE, 1000);
	hash_table_specify_function(process_table, HASH_FUNCTION_BYTE_SUM);

	process_list = ll_create(0);
	mutex_create(&process_refs_lock, 0);
	mutex_create(&thread_refs_lock, 0);
	
	thread_table = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(thread_table, HASH_RESIZE_MODE_IGNORE, 1000);
	hash_table_specify_function(thread_table, HASH_FUNCTION_BYTE_SUM);

	struct thread *thread = kmalloc(sizeof(struct thread));
	struct process *proc = kernel_process = kmalloc(sizeof(struct process));

	proc->refs = 2;
	thread->refs = 1;
	assert(!hash_table_set_entry(process_table, &proc->pid, sizeof(proc->pid), 1, proc));
	ll_do_insert(process_list, &proc->listnode, proc);

	valloc_create(&proc->mmf_valloc, MMF_BEGIN, MMF_END, PAGE_SIZE, VALLOC_USERMAP);
	for(addr_t a = MMF_BEGIN;a < (MMF_BEGIN + (size_t)proc->mmf_valloc.nindex);a+=PAGE_SIZE)
		mm_vm_set_attrib(a, PAGE_PRESENT | PAGE_WRITE);
	ll_create(&proc->threadlist);
	mutex_create(&proc->map_lock, 0);
	mutex_create(&proc->stacks_lock, 0);
	proc->magic = PROCESS_MAGIC;
	memcpy(&proc->vmm_context, &kernel_context, sizeof(kernel_context));
	thread->process = proc; /* we have to do this early, so that the vmm system can use the lock... */
	thread->state = THREADSTATE_RUNNING;
	thread->magic = THREAD_MAGIC;
	thread->kernel_stack = &initial_kernel_stack;
	*(struct thread **)(thread->kernel_stack) = thread;


	primary_cpu->active_queue = tqueue_create(0, 0);
	primary_cpu->idle_thread = thread;
	primary_cpu->numtasks=1;
	ticker_create(&primary_cpu->ticker, 0);
	workqueue_create(&primary_cpu->work, 0);
	tm_thread_add_to_process(thread, proc);
	tm_thread_add_to_cpu(thread, primary_cpu);
	add_atomic(&running_processes, 1);
	add_atomic(&running_threads, 1);
	set_ksf(KSF_THREADING);
	primary_cpu->flags |= CPU_RUNNING;
#if CONFIG_MODULES
	loader_add_kernel_symbol(tm_thread_delay_sleep);
	//loader_add_kernel_symbol(tm_schedule);
#endif
}

void tm_thread_user_mode_jump(void (*fn)(void))
{
	cpu_set_kernel_stack(current_thread->cpu, (addr_t)current_thread->kernel_stack,
			(addr_t)current_thread->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	arch_tm_jump_to_user_mode((addr_t)fn);
}

