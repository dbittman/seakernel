#include <kernel.h>
#include <sea/syscall.h>
#include <isr.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <sys/stat.h>
#include <sys/sysconf.h>
#include <cpu.h>
#include <sea/uname.h>
long sys_sysconf(int cmd)
{
	int ret = -EINVAL;
	switch(cmd)
	{
		case _SC_PAGESIZE:
			ret = PAGE_SIZE;
			break;
		case _SC_CLK_TCK:
			ret = current_hz;
			break;
		case _SC_PHYS_PAGES:
			ret = pm_num_pages;
			break;
		case _SC_NGROUPS_MAX:
			ret = 1000;
			break;
		case _SC_CHILD_MAX:
			ret = 0;
			break;
		case _SC_OPEN_MAX:
			ret = FILP_HASH_LEN;
			break;
		case _SC_ARG_MAX:
			ret = 0x1000;
			break;
		case _SC_AVPHYS_PAGES:
			return pm_num_pages; 
			/* this is not correct. We should only return the number of pages
			 * we can use immediately without fucking over other things */
			break;
		case _SC_NPROCESSORS_ONLN:
#if CONFIG_SMP
			return num_booted_cpus + 1;
#else
			return 1; /* no SMP, thus only one processor */
#endif
			break;
		default:
			printk(1, "[sysconf]: %d gave unknown specifier: %d\n", 
					current_task->pid, cmd);
	}
	return ret;
}

int sys_gethostname(char *buf, size_t len)
{
	if(!buf || !len)
		return -EINVAL;
	strncpy(buf, "seaos", len);
	return 0;
}

int sys_uname(struct utsname *name)
{
	if(!name)
		return -EINVAL;
	strncpy(name->sysname, "seaos", 6);
	strncpy(name->nodename, "", 1);
	get_kernel_version(name->release);
	strncpy(name->version, "eclipse", 8);
#if CONFIG_ARCH == TYPE_ARCH_X86
	strncpy(name->machine, "i586", 5);
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
	strncpy(name->machine, "x86_64", 6);
#endif
	strncpy(name->domainname, "", 1);
	return 0;
}
