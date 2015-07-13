/* This file handles the kernel task. It has a couple of purposes: 
 * -> Provides a task to run if all other tasks have slept
 * -> Cleans up after tasks that tm_exit
 * Note: We want to spend as little time here as possible, since it's 
 * cleanup code can run slowly and when theres
 * nothing else to do. So we reschedule often.
 */
#include <sea/boot/multiboot.h>
#include <sea/tty/terminal.h>
#include <sea/mm/vmm.h>
#include <sea/asm/system.h>
#include <sea/tm/process.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/boot/init.h>
#include <sea/loader/symbol.h>
#include <sea/lib/cache.h>
#include <sea/mm/swap.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/tm/kthread.h>

int __KT_try_releasing_tasks();
void __KT_try_handle_stage2_interrupts();

static inline int __KT_clear_args(void)
{
	/* Clear out the alloc'ed arguments */
	/* TODO */
#if 0
	if(next_pid > (unsigned)(init_pid+1) && init_pid)
	{
		printk(1, "[idle]: clearing unused kernel memory...\n");
		int w=0;
		for(;w<128;w++)
		{
			if(stuff_to_pass[w] && (addr_t)stuff_to_pass[w] > KMALLOC_ADDR_START)
				kfree(stuff_to_pass[w]);
		}
		return 1;
	}
#endif
	return 1;
}

struct kthread kthread_pager;

int kt_kernel_idle_task(void)
{
	int task, cache;
	kthread_create(&kthread_pager, "[kpager]", 0, __KT_pager, 0);
	current_thread->flags |= THREAD_KERNEL;
	strncpy((char *)current_process->command, "[kidle]", 128);
	/* First stage is to wait until we can clear various allocated things
	 * that we wont need anymore */
	while(!__KT_clear_args())
	{
		tm_schedule();
		cpu_interrupt_set(1);
	}
	cpu_interrupt_set(0);
	printk(1, "[kernel]: remapping lower memory with protection flags...\n");
	addr_t addr = 0;
	while(addr != TOP_LOWER_KERNEL)
	{
		/* set it to write. We don't actually have to do this, because
		 * ring0 code may always access memory. As long as the PAGE_USER
		 * flag isn't set... */
		if(!(SIGNAL_INJECT >= addr && SIGNAL_INJECT < (addr + PAGE_SIZE_LOWER_KERNEL)))
			mm_vm_set_attrib(addr, PAGE_PRESENT | PAGE_WRITE);
		addr += PAGE_SIZE_LOWER_KERNEL;
	}
	cpu_interrupt_set(1);
	/* Now enter the main idle loop, waiting to do periodic cleanup */
	printk(0, "[idle]: entering background loop\n");
	for(;;) {
		tm_schedule();
		if(__current_cpu->work.count > 0)
			workqueue_dowork(&__current_cpu->work);

	}
}

