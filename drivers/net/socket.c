/* provides functions for managing and accessing socket devices 
 * copyright 2013 Daniel Bittman */

#include <kernel.h>
#include <mod.h>

int module_install()
{
	printk(1, "[socket]: installing socket devices\n");
	
	return 0;
}

int module_exit()
{
	printk(1, "[socket]: removing socket devices\n");
	
	return 0;
}
