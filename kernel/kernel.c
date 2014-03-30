/* kernel.c: Copyright (c) 2010 Daniel Bittman
 * Provides some fairly standard functions for the kernel */
#include <sea/kernel.h>
#include <asm/system.h>
#include <sea/loader/module.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/mount.h>
#include <sea/cpu/interrupt.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <cpu-x86.h>
#else
#include <cpu-x86_64.h>
#endif
int sys_sync(int);
void acpiPowerOff(void);
int PRINT_LEVEL = DEF_PRINT_LEVEL;
volatile unsigned kernel_state_flags=0;

void kernel_shutdown()
{
	interrupt_set(0);
#if CONFIG_SMP
	printk(0, "[smp]: shutting down application processors\n");
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_SHUTDOWN);
	while(num_halted_cpus < num_booted_cpus) asm("pause");
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
		asm("nop");
}
