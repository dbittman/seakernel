/* init/main.c: Copyright (c) 2010 Daniel Bittman
 * Provides initialization functions for the kernel */
#include <kernel.h>
#include <multiboot.h>
#include <console.h>
#include <memory.h>
#include <asm/system.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include <init.h>
#include <mod.h>
#include <cache.h>
#include <mod.h>
#include <elf.h>

struct multiboot *mtboot;
addr_t i_stack=0;

char init_path[128] = "*";
char root_device[64] = "/";
char *stuff_to_pass[128];
int argc_STP=3;
int count_ie=0;
char *init_env[12];
char cleared_args=0;
unsigned init_pid=0;
char kernel_name[128];
elf32_t kernel_elf;
int april_fools=0;
struct tm kernel_start_time;

void parse_kernel_cmd(char *buf)
{
	char *current = buf;
	char *tmp;
	unsigned argc=0;
	char a[128];
	int type=0;
	int init_mods=0;
	memset(stuff_to_pass, 0, 128 * sizeof(char *));
	while(current && *current)
	{
		tmp = strchr(current, ' ');
		memset(a, 0, 128);
		addr_t len = (addr_t)tmp ? (addr_t)(tmp-current) 
			: (addr_t)strlen(current);
		strncpy(a, current, len >= 128 ? 127 : len);
		if(!argc)
		{
			memset(kernel_name, 0, 128);
			strncpy(kernel_name, a, 128);
		} else if(!type) {
			if(!strncmp("init=\"", a, 6))
			{
				strncpy(init_path, a+6, 128);
				init_path[strlen(init_path)-1]=0;
				printk(KERN_INFO, "[kernel]: init=%s\n", init_path);
			}
			else if(!strncmp("root=\"", a, 6))
			{
				memset(root_device, 0, 64);
				strncpy(root_device, a+6, 64);
				root_device[strlen(root_device)-1]=0;
				printk(KERN_INFO, "[kernel]: root=%s\n", root_device);
			}
			else if(!strcmp("aprilfools", a))
				april_fools = !april_fools;
			else if(!strncmp("loglevel=", a, 9))
			{
				char *lev = ((char *)a) + 9;
				int logl = strtoint(lev);
				printk(1, "[kernel]: Setting loglevel to %d\n", logl);
				PRINT_LEVEL = logl;
			} else {
				stuff_to_pass[argc_STP] = (char *)kmalloc(strlen(a)+1);
				_strcpy(stuff_to_pass[argc_STP++], a);
			}
		} else
		{
			/* switch type */
			type=0;
		}
		if(!tmp)
			break;
		argc++;
		current = tmp+1;
	}
	stuff_to_pass[0] = (char *)kmalloc(9);
	_strcpy(stuff_to_pass[0], "ird-sh");
	stuff_to_pass[1] = (char *)kmalloc(9);
	_strcpy(stuff_to_pass[1], "-c");
	stuff_to_pass[2] = (char *)kmalloc(90);
	sprintf(stuff_to_pass[2], "/preinit.sh %s", root_device);
	
}

/* This is the C kernel entry point */
void kmain(struct multiboot *mboot_header, addr_t initial_stack)
{
	/* Store passed values, and initiate some early things
	 * We want serial log output as early as possible */
	kernel_state_flags=0;
	mtboot = mboot_header;
	i_stack = initial_stack;
#if CONFIG_ARCH == TYPE_ARCH_X86
	parse_kernel_elf(mboot_header, &kernel_elf);
#endif
#if CONFIG_MODULES
	init_kernel_symbols();
#endif
	init_serial();
	console_init_stage1();
	load_tables();
	puts("~ SeaOS Version ");	
	char ver[32];
	get_kernel_version(ver);
	puts(ver);
	puts(" Booting Up ~\n\r");
#if CONFIG_MODULES
	init_module_system();
#endif
	init_syscalls();
	load_initrd(mtboot);
	install_timer(1000);
	pm_init(placement, mtboot);
	init_main_cpu();

	/* Now get the management stuff going */
	printk(1, "[kernel]: Starting system management\n");
	#if CONFIG_ARCH == TYPE_ARCH_X86_64
	asm("sti");
	for(;;);
	#endif	
	init_memory(mtboot);
	console_init_stage2();
	parse_kernel_cmd((char *)(addr_t)mtboot->cmdline);
	init_multitasking();
	init_cache();
	init_dm();
	init_vfs();
	/* Load the rest... */
	process_initrd();
	init_kern_task();

	get_timed(&kernel_start_time);
	printk(KERN_MILE, "[kernel]: Kernel is setup (%2.2d:%2.2d:%2.2d, %s, kv=%d, ts=%d bytes: ok)\n", 
	       kernel_start_time.tm_hour, kernel_start_time.tm_min, 
	       kernel_start_time.tm_sec, kernel_name, KVERSION, sizeof(task_t));
	assert(!set_int(1));
	if(!fork())
		init();
	sys_setsid();
	enter_system(255);
	kernel_idle_task();
}

/* User-mode printf function */
void printf(const char *fmt, ...)
{
	char printbuf[1024];
	memset(printbuf, 0, 1024);
	va_list args;
	va_start(args, fmt);
	vsprintf(printbuf, fmt, args);
	u_write(1, printbuf);
	va_end(args);
}

void init()
{
	/* Call sys_setup. This sets up the root nodes, and filedesc's 0, 1 and 2. */
	sys_setup();
	kprintf("Something stirs and something tries, and starts to climb towards the light.\n");
	/* Set some basic environment variables. These allow simple root execution, 
	 * basic terminal access, and a shell to run from */
	add_init_env("PATH=/bin/:/usr/bin/:/usr/sbin:");
	add_init_env("TERM=seaos");
	add_init_env("HOME=/");
	add_init_env("SHELL=/bin/sh");
	int ret=0;
	int pid;
	init_pid = current_task->pid+1;
	switch_to_user_mode();
//#warning "MEMORY LEAK"
	//for(;;) {
	//	if(!u_fork()) {
	//		printf("Hello!\n");
	//		u_exit(0);
	//	}
		
	//}
	/* We have to be careful now. If we try to call any kernel functions
	 * without doing a system call, the processor will generate a GPF (or 
	 * a page fault) because you can't execute kernel code in ring 3!
	 * So we write simple wrapper functions for common functions that 
	 * we will need */
	ret = u_execve("/sh", (char **)stuff_to_pass, (char **)init_env);
	ret = u_execve("/bin/sh", (char **)stuff_to_pass, (char **)init_env);
	ret = u_execve("/usr/bin/sh", (char **)stuff_to_pass, (char **)init_env);
	printf("Failed to start the init process. Halting.\n");
	u_exit(0);
}
