/* kernel.c: Copyright (c) 2010 Daniel Bittman
 * Provides some fairly standard functions for the kernel */
#include <sea/kernel.h>
#include <sea/asm/system.h>
#include <sea/loader/module.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/mount.h>
#include <sea/cpu/interrupt.h>
#include <sea/asm/system.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif

int PRINT_LEVEL = DEF_PRINT_LEVEL;
volatile unsigned kernel_state_flags=0;

void kernel_shutdown()
{
	interrupt_set(0);
#if CONFIG_SMP
	printk(0, "[smp]: shutting down application processors\n");
	cpu_send_ipi(CPU_IPI_DEST_OTHERS, IPI_SHUTDOWN, 0);
	while(cpu_get_num_halted_processors() < cpu_get_num_secondary_processors()) asm("pause");
#endif
	current_task->thread->effective_uid=current_task->thread->real_uid=0;
	tm_raise_flag(TF_SHUTDOWN);
	set_ksf(KSF_SHUTDOWN);
	sys_sync(PRINT_LEVEL);
	fs_unmount_all();
#if CONFIG_MODULES
	loader_unload_all_modules(1);
#endif
	kprintf("Everything under the sun is in tune, but the sun is eclipsed by the moon.\n");
}

void kernel_reset()
{
	if(current_task->thread->effective_uid)
		return;
	kernel_shutdown();
	kprintf("Rebooting system...\n");
	do_reset();
}

void kernel_poweroff()
{
	if(current_task->thread->effective_uid)
		return;
	kernel_shutdown();
	interrupt_set(0);
	kprintf("\nYou can now turn off your computer.\n");
	for(;;) 
		arch_cpu_halt();
}
