#include <sea/kernel.h>
#include <sea/cpu/cpu-io.h>

unsigned char cmos_read(unsigned char addr)
{
	unsigned char ret;
	outb(0x70,addr);
	__asm__ volatile ("jmp 1f; 1: jmp 1f;1:");
	ret = inb(0x71);
	__asm__ volatile ("jmp 1f; 1: jmp 1f;1:");
	return ret;
}

void cmos_write(unsigned char addr, unsigned int value)
{
	outb(0x70, addr);
	__asm__ __volatile__ ("jmp 1f; 1: jmp 1f;1:");
	outb(0x71, value);
	__asm__ __volatile__ ("jmp 1f; 1: jmp 1f;1:");
}
