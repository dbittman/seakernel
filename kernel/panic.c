/* panic.c: Copyright (c) 2010 Daniel Bittman
 * Functions for kernel crashes and exceptions */
#include <sea/kernel.h>
#include <stdarg.h>
#include <sea/asm/system.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>

#include <sea/cpu/interrupt.h>
#include <sea/fs/file.h>
#include <sea/vsprintf.h>
#include <sea/debugger.h>
static inline void _set_lowercase(char *b)
{
	while(*b) {
		if(*b >= 'A' && *b <= 'Z')
			*b += 32;
		b++;
	}
}

static void __panic_print_extra_data(int flags, struct thread *t)
{
	if(t) 
		printk_safe(9,"current_task=%x:%d, sys=%d, flags=%x. Stack trace:\n", t, 
				t->tid, t->system, t->flags);
	cpu_print_stack_trace(64);
}

void panic(int flags, char *fmt, ...)
{
	cpu_interrupt_set(0);
#if CONFIG_SMP
	if(!(flags & PANIC_INSTANT)) {
		/* tell the other processors to halt */
		cpu_send_ipi(CPU_IPI_DEST_OTHERS, IPI_PANIC, 0);
		int timeout = 100000;
		while(cpu_get_num_halted_processors() 
				< cpu_get_num_secondary_processors() && --timeout)
			cpu_pause();
	}
#endif
	if(kernel_state_flags & KSF_PANICING) {
		/* panicing from within a panic? That's....bad.... */
		for(;;) {
			cpu_interrupt_set(0);
			cpu_halt();
		}
	}
	struct thread *t = current_thread;
	set_ksf(KSF_PANICING);
	
	printk_safe(9, "\n\n*** kernel panic - ");	
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(512, buf, fmt, args);
	va_end(args);
	_set_lowercase(buf);
	printk_safe(9,buf);
	printk_safe(9," ***\n");
	
	if(t && t->process->pid && !(flags & PANIC_NOSYNC) && !(flags & PANIC_INSTANT))
	{
		printk_safe(9,"[panic]: syncing...");
		//sys_sync();
		printk_safe(9,"\n[panic]: Done\n");
	} else
		printk_safe(9,"[panic]: not syncing\n");
	
	__panic_print_extra_data(flags, t);
#if CONFIG_GDB_STUB
	/* breakpoint so that GDB will catch us, allowing some better debugging */
	asm("int $0x3");
#endif
	debugger_enter();
	cpu_interrupt_set(0);
	for(;;)
		cpu_halt();
}

void panic_assert(const char *file, uint32_t line, const char *desc)
{
	panic(PANIC_NOSYNC, "Internal inconsistancy (%s @ %d): %s\n", file, line, desc);
}

