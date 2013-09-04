/* panic.c: Copyright (c) 2010 Daniel Bittman
 * Functions for kernel crashes and exceptions */
#include <kernel.h>
#include <stdarg.h>
#include <asm/system.h>
#include <task.h>
#include <cpu.h>
#include <atomic.h>

static inline void _set_lowercase(char *b)
{
	while(*b) {
		if(*b >= 'A' && *b <= 'Z')
			*b += 32;
		b++;
	}
}

void panic(int flags, char *fmt, ...)
{
	set_int(0);
#if CONFIG_SMP
	/* tell the other processors to halt */
	send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_PANIC);
#endif
	if(kernel_state_flags & KSF_PANICING) {
		for(;;) asm("cli; hlt");
	}
	set_ksf(KSF_PANICING);
	int pid=0;
	task_t *t=current_task;
	if(t) pid=t->pid;
	
	kprintf("\n\n*** kernel panic - ");
	
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	_set_lowercase(buf);
	kprintf(buf);
	
	kprintf(" ***\n");
	
	if(t) 
		kprintf("current_task=%x:%d, sys=%d, flags=%x, F=%x. Stack trace:\n", t, 
				t->pid, t->system, t->flags, t->flag);
	print_trace(10);
	if(pid && !(flags & PANIC_NOSYNC))
	{
		kprintf("[panic]: syncing...");
		sys_sync();
		kprintf("\n[panic]: Done\n");
	} else
		kprintf("[panic]: not syncing\n");
	for(;;)
	{
		asm("cli; hlt;");
	}
}

void panic_assert(const char *file, u32int line, const char *desc)
{
	panic(PANIC_NOSYNC, "Internal inconsistancy (%s @ %d): %s\n", file, line, desc);
}
