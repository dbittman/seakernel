/* Serial port access. Also, debugging output...*/
#include <kernel.h>
#include <asm/system.h>
#include <char.h>
#include <mutex.h>
mutex_t serial_m;
int ports[8] = {
	0x3f8,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};
int Iports[8] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

void init_serial_port(int PORT) {
	__super_cli();
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}
int serial_received(int p) {
	return inb(p + 5) & 1;
}

int read_serial(int port) {
	while (serial_received(port) == 0) {
		if(got_signal(current_task))
			return -EINTR;
	}
	
	return inb(port);
}

int is_transmit_empty(int p) {
	return inb(p + 5) & 0x20;
}

void write_serial(int p, char a) {
	while (is_transmit_empty(p) == 0);
	outb(p,a);
}

void serial_puts(int port, char *s)
{
	mutex_on(&serial_m);
	while(*s)
	{
		write_serial(ports[port], *s);
		s++;
	}
	mutex_off(&serial_m);
}

int serial_rw(int rw, int min, char *b, int c)
{
	if(!ports[min])
		return -EINVAL;
	int i=0;
	mutex_on(&serial_m);
	if(rw == WRITE)
	{
		while(c)
		{
			write_serial(ports[min], *(b+i));
			i++;
			c--;
		}
		mutex_off(&serial_m);
		return i;
	} else if(rw == READ)
	{
		while(c)
		{
			int r = read_serial(ports[min]);
			if(r == -EINTR) {
				mutex_off(&serial_m);
				return -EINTR;
			}
			b[i++] = (char)r;
			c--;
		}
		mutex_off(&serial_m);
		return i;
	} else if(rw == OPEN)
	{
		init_serial_port(ports[min]);
		Iports[min]=1;
		mutex_off(&serial_m);
		return 0;
	}
	return -EINVAL;
}

void init_serial()
{
	create_mutex(&serial_m);
	init_serial_port(ports[0]);
	Iports[0]=1;
	serial_puts(0, "[kernel]: started debug serial output\n");
}
