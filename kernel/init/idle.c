/* This file handles the kernel task. It has a couple of purposes: 
 * -> Provides a task to run if all other tasks have slept
 * -> Cleans up after tasks that tm_exit
 * Note: We want to spend as little time here as possible, since it's 
 * cleanup code can run slowly and when theres
 * nothing else to do. So we reschedule often.
 */
#include <sea/asm/system.h>
#include <sea/boot/init.h>
#include <sea/boot/multiboot.h>

#include <sea/cpu/interrupt.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/loader/symbol.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/vmm.h>
#include <sea/string.h>
#include <sea/tm/kthread.h>
#include <sea/tm/process.h>
#include <sea/vsprintf.h>
#include <sea/cpu/processor.h>

struct kthread kthread_pager;
int __KT_pager(struct kthread *, void *);
int kt_kernel_idle_task(void)
{
	tm_thread_raise_flag(current_thread, THREAD_KERNEL);
	kthread_create(&kthread_pager, "[kpager]", 0, __KT_pager, 0);
	strncpy((char *)current_process->command, "[kernel]", 128);
	/* wait until init has successfully executed, and then remap. */
	while(!(kernel_state_flags & KSF_HAVEEXECED)) {
		tm_schedule();
	}
	printk(1, "[kernel]: remapping lower memory with protection flags...\n");
	cpu_interrupt_set(0);
	for(addr_t addr = MEMMAP_KERNEL_START; addr < MEMMAP_KERNEL_END; addr += PAGE_SIZE)
	{
		mm_virtual_changeattr(addr, PAGE_PRESENT | PAGE_WRITE, PAGE_SIZE);
	}
	cpu_interrupt_set(1);
	/* Now enter the main idle loop, waiting to do periodic cleanup */
	printk(0, "[idle]: entering background loop\n");
	for(;;) {
		assert(!current_thread->held_locks);
		int r=1;
		if(__current_cpu->work.count > 0)
			r=workqueue_dowork(&__current_cpu->work);
		else
			tm_schedule();
		int status;
		int pid = sys_waitpid(-1, &status, WNOHANG);
		if(WIFSTOPPED(status)) {
			sys_kill(pid, SIGKILL);
		}
	}
}

