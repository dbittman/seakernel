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
	
	printk_safe(9, "\n\n*** kernel panic - ");
	
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	_set_lowercase(buf);
	printk_safe(9,buf);
	
	printk_safe(9," ***\n");
	
	if(t) 
		printk_safe(9,"current_task=%x:%d, sys=%d, flags=%x, F=%x. Stack trace:\n", t, 
				t->pid, t->system, t->flags, t->flag);
	print_trace(10);
	if(pid && !(flags & PANIC_NOSYNC))
	{
		printk_safe(9,"[panic]: syncing...");
		sys_sync();
		printk_safe(9,"\n[panic]: Done\n");
	} else
		printk_safe(9,"[panic]: not syncing\n");
	if(flags & PANIC_VERBOSE)
	{
		printk_safe(9, "task listing:\n");
		struct llistnode *cur;
		t=0;
		ll_for_each_entry(&primary_queue->tql, cur, task_t *, t)
		{
			printk_safe(9, "\ntask %5d: magic=%x, state=%d, flags=0x%x, F=%d, sys=%d\n", t->pid, t->magic, t->state, t->flags, t->flag, t->system);
			printk_safe(9, "          : cpu=%x (%d)\n", t->cpu, ((cpu_t *)(t->cpu))->apicid);
		}
	}
#if CONFIG_GDB_STUB
	/* breakpoint so that GDB will catch us, allowing some better debugging */
	asm("int $0x3");
#endif
	for(;;)
	{
		asm("cli; hlt;");
	}
}

void panic_assert(const char *file, u32int line, const char *desc)
{
	panic(PANIC_NOSYNC, "Internal inconsistancy (%s @ %d): %s\n", file, line, desc);
}
