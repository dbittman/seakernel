#ifndef _CPU_IO_x86_64_H
#define _CPU_IO_x86_64_H

static inline void outb(short port, char value)
{
	__asm__ volatile ("outb %1, %0" : : "dN" (port), "a" ((unsigned char)value));
}

static inline char inb(short port)
{
	char ret;
	__asm__ volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

static inline short inw(short port)
{
	short ret;
	__asm__ volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

static inline void outw(short port, short val)
{
	__asm__ volatile ("outw %1, %0" : : "dN" (port), "a" (val));
}

static inline void outl(short port, int val)
{
	__asm__ volatile ("outl %1, %0" : : "dN" (port), "a" (val));
}

static inline int inl(short port)
{
	int ret;
	__asm__ volatile ("inl %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

#endif
