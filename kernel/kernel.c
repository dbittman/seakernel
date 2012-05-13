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

void kernel_shutdown()
{
	current_task->uid=0;
	shutting_down=1;
	/** Write the system-down stuffs */
	/* Unload modules, unmount stuff, sync stuff, kill all tasks*/
	kill_all_tasks();
	kernel_task->next = current_task;
	current_task->next=0;
	current_task->prev = kernel_task;
	sys_sync(PRINT_LEVEL);
	unmount_all();
	unload_all_modules(1);
	kprintf("Everything under the sun is in tune, but the sun is eclipsed by the moon.\n");
}

void kernel_reset()
{
	kernel_shutdown();
	kprintf("Rebooting system...\n");
	do_reset();
}

void kernel_poweroff()
{
	kernel_shutdown();
	super_cli();
	acpiPowerOff();
	kprintf("\nYou can now turn off your computer.\n");
	for(;;) 
		asm("nop");
}
