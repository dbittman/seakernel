/* panic.c: Copyright (c) 2010 Daniel Bittman
 * Functions for kernel crashes and exceptions */
#include <kernel.h>
#include <vargs.h>
#include <asm/system.h>
#include <task.h>
#include <elf.h>
extern int vsprintf(char *buf, const char *fmt, va_list args);
volatile int panicing=0;
extern unsigned end;

static inline void _set_lowercase(char *b)
{
	while(*b) {
		if(*b >= 'A' && *b <= 'Z')
			*b += 32;
		b++;
	}
}

void print_trace(unsigned int MaxFrames)
{
	unsigned int * ebp = &MaxFrames - 2;
	for(unsigned int frame = 0; frame < MaxFrames; ++frame)
	{
		unsigned int eip = ebp[1];
		if(eip == 0)
			break;
		ebp = (unsigned int *)(ebp[0]);
		const char *name = elf_lookup_symbol(eip, &kernel_elf);
		if(name) kprintf("  <%x>  %s\n", eip, name);
	}
}

void panic(int flags, char *fmt, ...)
{
	cli();
	int second_panic = panicing++;
	int pid=0;
	task_t *t=current_task;
	if(t) pid=t->pid;
	set_current_task_dp(0, 0);
	kprintf("\n\n*** kernel panic (%d) - ", second_panic+1);
	
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
	if(pid && !second_panic && !(flags & PANIC_NOSYNC))
	{
		kprintf("[panic]: syncing...");
		sys_sync();
		kprintf("\n[panic]: Done\n");
	} else
		kprintf("[panic]: not syncing\n");
	cli();
	for(;;)
	{
		asm("cli; hlt;");
	}
}

void panic_assert(const char *file, u32int line, const char *desc)
{
	panic(PANIC_NOSYNC, "Internal inconsistancy (%s @ %d): %s\n", file, line, desc);
}
