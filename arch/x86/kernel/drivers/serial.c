/* Serial port access. This is the most basic driver that can only
 * output to the first serial port. For better access, you'll need a
 * module of some sort */
#include <kernel.h>
#include <asm/system-x86.h>
#include <char.h>
#include <mutex.h>
#include <mod.h>
mutex_t serial_m;
char serial_initialized=0;

#define serial_received(x) (inb(x+5)&0x01)
#define serial_transmit_empty(x) (inb(x+5)&0x20)

void init_serial_port(int PORT) 
{
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void write_serial(int p, char a) 
{
	while (serial_transmit_empty(p) == 0);
	outb(p,a);
}

void serial_puts(int port, char *s)
{
	if(!serial_initialized)
		return;
	mutex_acquire(&serial_m);
	while(*s)
	{
		write_serial(0x3f8, *s);
		s++;
	}
	mutex_release(&serial_m);
}

void serial_puts_nolock(int port, char *s)
{
	if(!serial_initialized)
		return;
	while(*s)
	{
		write_serial(0x3f8, *s);
		s++;
	}
}

int serial_rw(int rw, int min, char *b, size_t c)
{
	if(!serial_initialized) 
		return -EINVAL;
	int i=c;
	if(rw == WRITE)
	{
		mutex_acquire(&serial_m);
		while(i)
		{
			write_serial(0x3f8, *(b++));
			i--;
		}
		mutex_release(&serial_m);
		return c;
	}
	return 0;
}

void init_serial()
{
	mutex_create(&serial_m, 0);
	init_serial_port(0x3f8);
	serial_initialized = 1;
	serial_puts_nolock(0, "[kernel]: started debug serial output\n");
#if CONFIG_MODULES
	add_kernel_symbol(serial_puts);
#endif
}
