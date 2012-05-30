#include <kernel.h>
#include <time.h>
unsigned long long get_epoch_time();
#define BCD2BIN(bcd) ((((bcd)&15) + ((bcd)>>4)*10))

unsigned char readCMOS(unsigned char addr)
{
	unsigned char ret;
	outb(0x70,addr);
	__asm__ volatile ("jmp 1f; 1: jmp 1f;1:");
	ret = inb(0x71);
	__asm__ volatile ("jmp 1f; 1: jmp 1f;1:");
	return ret;
}

void writeCMOS(unsigned char addr, unsigned int value)
{
	outb(0x70, addr);
	__asm__ __volatile__ ("jmp 1f; 1: jmp 1f;1:");
	outb(0x71, value);
	__asm__ __volatile__ ("jmp 1f; 1: jmp 1f;1:");
}

void get_timed(struct tm *now) 
{
	now->tm_sec = BCD2BIN(readCMOS(0x0));
	now->tm_min = BCD2BIN(readCMOS(0x2));
	now->tm_hour = BCD2BIN(readCMOS(0x4));
	now->tm_mday = BCD2BIN(readCMOS(0x7));
	now->tm_mon = BCD2BIN(readCMOS(0x8));
	now->tm_year = BCD2BIN(readCMOS(0x9));
	now->tm_mon--;
}

void get_time(struct tm *now) {
	memset(now, 0, sizeof(struct tm));
	now->tm_sec = get_epoch_time();
}

unsigned long long get_epoch_time()
{
	struct tm *tm, _t;tm = &_t;
	get_timed(tm);
	tm->tm_year += 30;
	return ((((unsigned long)
		  (tm->tm_year/4 - tm->tm_year/100 + tm->tm_year/400 + 367*tm->tm_mon/12 + tm->tm_mday) +
		  tm->tm_year*365
	    )*24 + tm->tm_hour /* now have hours */
	  )*60 + tm->tm_min /* now have minutes */
	)*60 + tm->tm_sec; /* finally seconds */
}
