/* kernel.c: Copyright (c) 2010 Daniel Bittman
 * Provides some fairly standard functions for the kernel */
#include <kernel.h>
#include <asm/system.h>
#include <module.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>
#include <mount.h>

int sys_sync(int);
void acpiPowerOff(void);
int PRINT_LEVEL = DEF_PRINT_LEVEL;
volatile unsigned kernel_state_flags=0;

void kernel_shutdown()
{
	set_int(0);
#if CONFIG_SMP
	printk(0, "[smp]: shutting down application processors\n");
	send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_SHUTDOWN);
	while(num_halted_cpus < num_booted_cpus) asm("pause");
#endif
	current_task->thread->uid=0;
	raise_flag(TF_SHUTDOWN);
	set_ksf(KSF_SHUTDOWN);
	sys_sync(PRINT_LEVEL);
	unmount_all();
#if CONFIG_MODULES
	unload_all_modules(1);
#endif
	kprintf("Everything under the sun is in tune, but the sun is eclipsed by the moon.\n");
}

void kernel_reset()
{
	if(current_task->thread->uid)
		return;
	kernel_shutdown();
	kprintf("Rebooting system...\n");
	do_reset();
}

void kernel_poweroff()
{
	if(current_task->thread->uid)
		return;
	kernel_shutdown();
	set_int(0);
	kprintf("\nYou can now turn off your computer.\n");
	for(;;) 
		asm("nop");
}
