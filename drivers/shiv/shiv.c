#include <sea/kernel.h>
#include <sea/loader/module.h>

#include <modules/shiv.h>

int module_install()
{
	kprintf("SHIV - starting up\n");
	return 0;
}

int module_exit()
{
	return 0;
}

