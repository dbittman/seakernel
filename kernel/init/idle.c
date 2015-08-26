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

struct kthread kthread_pager;

int kt_kernel_idle_task(void)
{
	tm_thread_raise_flag(current_thread, THREAD_KERNEL);
	kthread_create(&kthread_pager, "[kpager]", 0, __KT_pager, 0);
	strncpy((char *)current_process->command, "[kernel]", 128);
	/* First stage is to wait until we can clear various allocated things
	 * that we wont need anymore */
	cpu_interrupt_set(0);
	printk(1, "[kernel]: remapping lower memory with protection flags...\n");
	addr_t addr = 0;
	while(addr != TOP_LOWER_KERNEL && 0) /* TODO: wait for init to be ready for this */
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
	printk(0, "[idle]: entering background loop %x\n", current_thread->kernel_stack);
	for(;;) {
		if(__current_cpu->work.count > 0)
			workqueue_dowork(&__current_cpu->work);
		else
			tm_schedule();
		sys_waitpid(-1, 0, WNOHANG);
	}
}

