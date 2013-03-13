/* kernel.c: Copyright (c) 2010 Daniel Bittman
 * Provides some fairly standard functions for the kernel */
#include <kernel.h>
#include <asm/system.h>
#include <mod.h>
#include <task.h>

char tables=0;
char shutting_down=0;
int sys_sync(int);
void acpiPowerOff(void);
int PRINT_LEVEL = DEF_PRINT_LEVEL;
unsigned kernel_state_flags=0;

void kernel_shutdown()
{
	current_task->uid=0;
	shutting_down=1;
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
