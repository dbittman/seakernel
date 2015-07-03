#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
#include <sea/tm/tqueue.h>
#include <sea/tm/context.h>
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

	primary_cpu->ktask = task;
	primary_cpu->numtasks=1;
	/* make this the "current_task" by assigning a specific location
	 * in the page directory as the pointer to the task. */
	arch_tm_set_current_task_marker((addr_t *)kernel_dir, (addr_t)task);
	/* this is the final thing to allow the system to begin scheduling
	 * once interrupts are enabled */
	primary_cpu->flags |= CPU_TASK;
	
	add_atomic(&running_processes, 1);
#if CONFIG_MODULES
	loader_add_kernel_symbol(tm_delay);
	loader_add_kernel_symbol(tm_delay_sleep);
	loader_add_kernel_symbol(tm_schedule);
	loader_add_kernel_symbol(tm_exit);
	loader_add_kernel_symbol(tm_get_ticks);
	loader_add_kernel_symbol(tm_get_current_frequency);
	loader_add_kernel_symbol(sys_setsid);
	loader_add_kernel_symbol(tm_do_fork);
	loader_add_kernel_symbol(tm_kill_process);
	loader_add_kernel_symbol(tm_do_send_signal);
	loader_add_kernel_symbol(dosyscall);
	loader_add_kernel_symbol(tm_process_pause);
	loader_add_kernel_symbol(tm_process_resume);
	loader_add_kernel_symbol(tm_process_got_signal);
	loader_add_kernel_symbol(kthread_create);
	loader_add_kernel_symbol(kthread_destroy);
	loader_add_kernel_symbol(kthread_wait);
	loader_add_kernel_symbol(kthread_join);
	loader_add_kernel_symbol(kthread_kill);
 #if CONFIG_SMP
	loader_add_kernel_symbol(cpu_get);
 #endif
	loader_do_add_kernel_symbol((addr_t)(task_t **)&kernel_task, "kernel_task");
#endif
}

void tm_set_current_task_marker(page_dir_t *space, addr_t task)
{
	arch_tm_set_current_task_marker(space, task);
}

void tm_set_kernel_stack(addr_t start, addr_t end)
{
	arch_tm_set_kernel_stack(start, end);
}

void tm_switch_to_user_mode()
{
	/* set up the kernel stack first...*/
	tm_set_kernel_stack(current_task->kernel_stack,
			current_task->kernel_stack + (KERN_STACK_SIZE-STACK_ELEMENT_SIZE));
	arch_tm_switch_to_user_mode();
}

