#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/schedule.h>
#include <sea/tm/kthread.h>
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

void tm_init_multitasking()
{
	printk(KERN_DEBUG, "[sched]: Starting multitasking system...\n");
	
	/* TODO: set up initial processes and threads */

	process_table = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(process_table, HASH_RESIZE_MODE_IGNORE, 1000);
	hash_table_specify_function(process_table, HASH_FUNCTION_BYTE_SUM);
	
	thread_table = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(thread_table, HASH_RESIZE_MODE_IGNORE, 1000);
	hash_table_specify_function(thread_table, HASH_FUNCTION_BYTE_SUM);

	//primary_cpu->idle_thread = thread;
	primary_cpu->numtasks=1;
	/* this is the final thing to allow the system to begin scheduling
	 * once interrupts are enabled */
	primary_cpu->flags |= CPU_TASK;
	
	add_atomic(&running_processes, 1);
	add_atomic(&running_threads, 1);
}

void tm_set_kernel_stack(addr_t start, addr_t end)
{
	arch_tm_set_kernel_stack(start, end);
}

void tm_switch_to_user_mode()
{
	/* set up the kernel stack first...*/
	tm_set_kernel_stack(current_thread->kernel_stack,
			current_thread->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	arch_tm_switch_to_user_mode();
}

