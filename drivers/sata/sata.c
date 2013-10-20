#include <kernel.h>
#include <module.h>
#include <modules/sata.h>

int module_install()
{
	printk(0, "[sata]: initializing sata driver...\n");
	return 0;
}

int module_exit()
{
	
	return 0;
}
