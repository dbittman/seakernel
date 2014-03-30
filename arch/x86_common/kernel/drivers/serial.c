/* Serial port access. This is the most basic driver that can only
 * output to the first serial port. For better access, you'll need a
 * module of some sort */
#include <kernel.h>
#include <asm/system.h>
#include <char.h>
#include <sea/mutex.h>
#include <sea/loader/symbol.h>
#include <task.h>
mutex_t serial_m;
char serial_initialized=0;

#define serial_received(x) (inb(x+5)&0x01)
#define serial_transmit_empty(x) (inb(x+5)&0x20)

#if ! CONFIG_SERIAL_DEBUG
#define DISABLE_SERIAL 1
#endif

#if DISABLE_SERIAL
#define DS_RET return
#else
#define DS_RET if(serial_debug_port == 0) return
#endif

int serial_debug_port = 0;

void init_serial_port(int PORT) 
{DS_RET;
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 12);    // Set divisor to 12 (lo byte) 9600 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void write_serial(int p, char a) 
{DS_RET;
	while (serial_transmit_empty(p) == 0);
	outb(p,a);
}

char read_serial(int p)
{DS_RET 0;
	while (serial_received(p) == 0);
	return inb(p);
}

void serial_console_puts(int port, char *s)
{DS_RET;
	if(!serial_initialized)
		return;
	mutex_acquire(&serial_m);
	while(*s)
	{
		write_serial(serial_debug_port, *s);
		s++;
	}
	mutex_release(&serial_m);
}

void serial_console_puts_nolock(int port, char *s)
{DS_RET;
	if(!serial_initialized)
		return;
	while(*s)
	{
		write_serial(serial_debug_port, *s);
		s++;
	}
}

int serial_rw(int rw, int min, char *b, size_t c)
{DS_RET c;
	if(!serial_initialized) 
		return -EINVAL;
	int i=c;
	if(rw == WRITE)
	{
		mutex_acquire(&serial_m);
		while(i)
		{
			write_serial(serial_debug_port, *(b++));
			i--;
		}
		mutex_release(&serial_m);
		return c;
	} else if(rw == READ)
	{
		while(i--) {
			while(serial_received(serial_debug_port) == 0)
			{
				if(tm_process_got_signal(current_task))
					return -EINTR;
			}
			*(b) = inb(serial_debug_port);
			kprintf("GOT: %x\n", *(b));
			b++;
		}
		return c;
	}
	return 0;
}

void init_serial()
{
#if ! DISABLE_SERIAL
	mutex_create(&serial_m, 0);
	/* BIOS data area */
	serial_debug_port = *(unsigned short *)(0x400);
	init_serial_port(serial_debug_port);
	serial_initialized = 1;
	serial_console_puts_nolock(0, "[kernel]: started debug serial output\n");
#endif
#if CONFIG_MODULES
	loader_add_kernel_symbol(serial_console_puts);
	loader_add_kernel_symbol(serial_console_puts_nolock);
#endif
}
