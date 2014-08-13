/* Serial port access. This is the most basic driver that can only
 * output to the first serial port. For better access, you'll need a
 * module of some sort */
#include <sea/kernel.h>
#include <sea/asm/system.h>
#include <sea/dm/char.h>
#include <sea/mutex.h>
#include <sea/loader/symbol.h>
#include <sea/tm/process.h>
#include <sea/tm/process.h>
#include <sea/cpu/cpu-io.h>

#if DISABLE_SERIAL
#define DS_RET return
#else
#define DS_RET if(serial_debug_port == 0) return
#endif

#define serial_received(x) (inb(x+5)&0x01)
#define serial_transmit_empty(x) (inb(x+5)&0x20)

static unsigned short ports[4] = {
	0x3F8,
	0x2F8,
	0x3E8,
	0x2E8
};

char arch_serial_received(int minor)
{
	if(minor >= 4) return 0;
	return inb(ports[minor] + 5) & 0x01;
}

static void init_serial_port(int PORT) 
{
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 12);      // Set divisor to 12 (lo byte) 9600 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void arch_serial_write(int minor, char a) 
{
	if(minor >= 4) return;
	int p = ports[minor];
	while (serial_transmit_empty(p) == 0);
	outb(p,a);
}

char arch_serial_read(int minor)
{
	if(minor >= 4) return 0;
	int p = ports[minor];
	while (serial_received(p) == 0);
	return inb(p);
}

void arch_serial_init(int *serial_debug_port_minor, int *serial_enable)
{
	/* BIOS data area */
	int serial_debug_port = *(unsigned short *)(0x400);
	if(serial_debug_port) {
		init_serial_port(serial_debug_port);
		int i;
		for(i=0;i<4;i++) {
			if(serial_debug_port == ports[i])
				break;
		}
		*serial_debug_port_minor = i;
		*serial_enable = 1;
	} else {
		*serial_debug_port_minor = *serial_enable = 0;
	}
}
