/* kernel.c: Copyright (c) 2010 Daniel Bittman
 * Provides some fairly standard functions for the kernel */
#include <kernel.h>
#include <asm/system.h>
#include <mod.h>
#include <task.h>
#include <cpu.h>

int sys_sync(int);
void acpiPowerOff(void);
int PRINT_LEVEL = DEF_PRINT_LEVEL;
unsigned kernel_state_flags=0;

void kernel_shutdown()
{
#if CONFIG_SMP
	send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_SHUTDOWN);
#endif
	current_task->uid=0;
	kernel_state_flags |= KSF_SHUTDOWN;
	cli();
	lock_scheduler();
	sys_sync(PRINT_LEVEL);
	unmount_all();
#if CONFIG_MODULES
	unload_all_modules(1);
#endif
	kprintf("Everything under the sun is in tune, but the sun is eclipsed by the moon.\n");
}

void kernel_reset()
{
	if(current_task->uid)
		return;
	kernel_shutdown();
	kprintf("Rebooting system...\n");
	do_reset();
}

void kernel_poweroff()
{
	if(current_task->uid)
		return;
	kernel_shutdown();
	cli();
	acpiPowerOff();
	kprintf("\nYou can now turn off your computer.\n");
	for(;;) 
		asm("nop");
}
