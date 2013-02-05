/* pit.c: Copyright (c) 2010 Daniel Bittman
 * Functions for handling the PIT
 */
#include <kernel.h>
#include <isr.h>
#include <task.h>
#include <atomic.h>
int current_hz=1000;
volatile long ticks=0;
void do_tick();

static void timer_handler(registers_t r)
{
	add_atomic(&ticks, 1);
	/* engage the idle task occasionally */
	if((ticks % current_hz*10) == 0)
		__engage_idle();
	do_tick();
}

void install_timer(int hz)
{
	current_hz=hz;
	register_interrupt_handler(IRQ0, &timer_handler);
	u32int divisor = 1193180 / hz;
	outb(0x43, 0x36);
	u8int l = (u8int)(divisor & 0xFF);
	u8int h = (u8int)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
}
