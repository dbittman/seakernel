/* init/main.c: Copyright (c) 2010 Daniel Bittman
 * Provides initialization functions for the kernel */
#include <sea/kernel.h>
#include <sea/boot/multiboot.h>
#include <sea/tty/terminal.h>
#include <sea/mm/vmm.h>
#include <sea/asm/system.h>
#include <sea/tm/process.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/boot/init.h>
#include <sea/lib/cache.h>
#include <sea/loader/symbol.h>
#include <sea/loader/elf.h>
#include <sea/cpu/processor.h>
#include <sea/mm/init.h>
#include <sea/dm/dev.h>
#include <sea/fs/initrd.h>
#include <sea/cpu/interrupt.h>
#include <sea/serial.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>
#include <stdarg.h>
#include <sea/mm/kmalloc.h>
#include <sea/trace.h>
#include <sea/tm/thread.h>

static struct multiboot *mtboot;
static time_t start_epoch;
static char *root_device = "/";
static char *init_path = "/bin/sh";
elf32_t kernel_elf;
addr_t initial_boot_stack=0;

static void parse_kernel_command_line(char *buf)
{
	char *c = buf;
	while(c && *c) {
		char *tmp = strchr(c, ' ');
		if(tmp) *tmp++ = 0;

		char *val = strchr(c, '=');
		if(!val) {
			printk(5, "[kernel]: malformed option: %s\n", c);
			c = tmp;
			continue;
		}

		*val++ = 0;
		if(!strcmp(c, "init")) {
			init_path = val;
		} else if(!strcmp(c, "root")) {
			root_device = val;
		} else if(!strcmp(c, "loglevel")) {
			PRINT_LEVEL = strtoint(val);
		} else if(!strcmp(c, "serial")) {
			if(!strcmp(val, "off"))
				serial_disable();
		} else {
			printk(0, "[kernel]: unknown option: %s=%s\n", c, val);
		}

		c = tmp;
	}
	printk(1, "[kernel]: root=%s\n", root_device);
	printk(1, "[kernel]: init=%s\n", init_path);
}

static void user_mode_init(void);
/* This is the C kernel entry point */
void kmain(struct multiboot *mboot_header, addr_t initial_stack)
{
	/* Store passed values, and initiate some early things
	 * We want serial log output as early as possible */
	kernel_state_flags=0;
	mtboot = mboot_header;
	initial_boot_stack = initial_stack;
	loader_parse_kernel_elf(mboot_header, &kernel_elf);
#if CONFIG_MODULES
	loader_init_kernel_symbols();
#endif
	serial_init();
	console_init_stage1();
	cpu_early_init();
	console_kernel_puts("~ SeaOS Version ");	
	console_kernel_puts(CONFIG_VERSION_STRING);
	console_kernel_puts(" Booting Up ~\n\r");
#if CONFIG_MODULES
	loader_init_modules();
#endif
	syscall_init();
	fs_initrd_load(mtboot);
	cpu_timer_install(1000);
	mm_pm_init(placement, mtboot);
	cpu_processor_init_1();

	/* Now get the management stuff going */
	printk(1, "[kernel]: Starting system management\n");
	mm_init(mtboot);
	console_init_stage2();
	parse_kernel_command_line((char *)(addr_t)mtboot->cmdline);
	tm_init_multitasking();
	dm_init();
	fs_init();
	net_init();
	trace_init();
	/* Load the rest... */
	printk(KERN_MILE, "[kernel]: Kernel is setup (kv=%d, bpl=%d: ok)\n", 
	       CONFIG_VERSION_NUMBER, BITS_PER_LONG);
	printk(KERN_DEBUG, "[kernel]: structure sizes: process=%d bytes, thread=%d bytes, inode=%d bytes\n",
			sizeof(struct process), sizeof(struct thread), sizeof(struct inode));
	cpu_interrupt_set(1);
	sys_setup();
	cpu_processor_init_2();
#if CONFIG_SMP
	cpu_boot_all_aps();
#endif
	if(!sys_clone(0)) {
		tm_thread_user_mode_jump(user_mode_init);
	}

	sys_setsid();
	kt_kernel_idle_task();
}

/* this function must exist entirely within ring 3. Because the kernel
 * code is readable by userspace until after the init process starts,
 * we don't have to worry about it faulting there. Besides that, everything
 * this function does is either on the stack or just in code, so none
 * of it will cause problems */
static void printf(const char *fmt, ...)
{
	char printbuf[1024];
	memset(printbuf, 0, 1024);
	va_list args;
	va_start(args, fmt);
	vsnprintf(1024, printbuf, fmt, args);
	u_write(1, printbuf);
	va_end(args);
}

static void user_mode_init(void)
{
	/* We have to be careful now. If we try to call any kernel functions
	 * without doing a system call, the processor will generate a GPF (or 
	 * a page fault) because you can't do fancy kernel stuff in ring 3!
	 * So we write simple wrapper functions for common functions that 
	 * we will need */
	char *init_env[5] = {
		"PATH=/bin/:/usr/bin/:/usr/sbin:",
		"TERM=seaos",
		"HOME=/",
		"SHELL=/bin/sh",
		NULL
	};
	int ret;
	char *init_argv[4] = {
		"sh",
		"/preinit.sh",
		root_device,
		NULL
	};
	ret = u_execve(init_path, (char **)init_argv, (char **)init_env);
	ret = u_execve("/sh", (char **)init_argv, (char **)init_env);
	ret = u_execve("/bin/sh", (char **)init_argv, (char **)init_env);
	printf("Failed to start the init process (err=%d). Halting.\n", -ret);
	u_exit(0);
}

