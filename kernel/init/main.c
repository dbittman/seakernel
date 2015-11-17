/* init/main.c: Copyright (c) 2010 Daniel Bittman
 * Provides initialization functions for the kernel */
#include <sea/asm/system.h>
#include <sea/boot/init.h>
#include <sea/boot/multiboot.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/dm/dev.h>
#include <sea/fs/initrd.h>
#include <sea/fs/inode.h>
#include <sea/kernel.h>
#include <sea/lib/timer.h>
#include <sea/loader/elf.h>
#include <sea/loader/symbol.h>
#include <sea/mm/kmalloc.h>
#include <sea/mm/vmm.h>
#include <sea/serial.h>
#include <sea/tm/process.h>
#include <sea/tm/thread.h>
#include <sea/trace.h>
#include <sea/tty/terminal.h>
#include <sea/vsprintf.h>
#include <stdarg.h>

static struct multiboot *mtboot;
static time_t start_epoch;
static char *root_device = "/";
static char *init_path = "/bin/sh";
#if CONFIG_SMP
static bool boot_cpus = true;
#endif
elf64_t kernel_elf;
addr_t initial_boot_stack=0;

static void parse_kernel_command_line(char *buf)
{
	char *c = buf + PHYS_PAGE_MAP; //TODO: something like mm_physical_read
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
#if CONFIG_SMP
		} else if(!strcmp(c, "smp")) {
			if(!strcmp(val, "off"))
				boot_cpus = false;
#endif
		} else {
			printk(0, "[kernel]: unknown option: %s=%s\n", c, val);
		}

		c = tmp;
	}
	printk(1, "[kernel]: root=%s\n", root_device);
	printk(1, "[kernel]: init=%s\n", init_path);
}

void __init_entry(void);
static void user_mode_init(void);
#include <sea/mm/map.h>
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
	cpu_early_init();
#if CONFIG_MODULES
	loader_init_modules();
#endif
	syscall_init();
	fs_initrd_load(mtboot);
	cpu_timer_install(1000);
	cpu_processor_init_1();

	/* Now get the management stuff going */
	printk(1, "[kernel]: Starting system management\n");
	mm_init(mtboot);
	syslog_init();
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
	timer_calibrate();
#if CONFIG_SMP
	if(boot_cpus)
		cpu_boot_all_aps();
#endif
	tm_clone(0, __init_entry, 0);
	sys_setsid();
	kt_kernel_idle_task();
}

void __init_entry(void)
{
	/* the kernel doesn't have this mapping, so we have to create it here. */
	tm_thread_raise_flag(current_thread, THREAD_KERNEL);
	addr_t ret = mm_mmap(current_thread->usermode_stack_start, CONFIG_STACK_PAGES * PAGE_SIZE,
			PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
	tm_thread_lower_flag(current_thread, THREAD_KERNEL);
	tm_thread_user_mode_jump(user_mode_init);
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
	char *init_argv[3] = {
		"init",
		root_device,
		NULL
	};
	ret = u_execve("/init", (char **)init_argv, (char **)init_env);
	printf("Failed to start the init process (err=%d). Halting.\n", -ret);
	u_exit(0);
}

