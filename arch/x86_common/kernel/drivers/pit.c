/* pit.c: Copyright (c) 2010 Daniel Bittman
 * Functions for handling the PIT
 */
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/schedule.h>
void do_tick();

void arch_cpu_timer_install(int hz)
{
	current_hz=hz;
	arch_interrupt_register_handler(IRQ0, &tm_timer_handler, 0);
	u32int divisor = 1193180 / hz;
	outb(0x43, 0x36);
	u8int l = (u8int)(divisor & 0xFF);
	u8int h = (u8int)( (divisor>>8) & 0xFF );
	outb(0x40, l);
	outb(0x40, h);
}
