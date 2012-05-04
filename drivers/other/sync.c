#include <kernel.h>
#include <task.h>
#include <cache.h>
#include <fs.h>
#include <mod.h>
int pid;
struct inode *exe=0;
int module_install()
{
	printk(1, "[sync]: Kernel service for autosyncronization beginning in 3 seconds\n");
	int x = mod_fork(&pid);
	if(!x)
	{
		sys_setsid();
		exe=set_as_kernel_task("ksync");
		delay(3000);
		printk(1, "[sync]: Autosync enabled\n");
		for(;;) {
			delay(1000);
			//kernel_cache_sync_slow(0);
		}
	}
	return 0;
}

int module_exit()
{
	if(exe) iremove_force(exe);
	if(!pid)
		printk(1, "[sync]: Warning - invalid PID in sync module\n");
	kill_task(pid);
	printk(1, "[sync]: Autosync disabled\n");
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
