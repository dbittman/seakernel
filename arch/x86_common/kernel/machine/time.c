#include <sea/cpu/time.h>
#include <sea/cpu/cmos-x86_common.h>
#include <sea/string.h>

#define BCD2BIN(bcd) ((((bcd)&15) + ((bcd)>>4)*10))

static void get_timed(struct tm *now) 
{
	now->tm_sec = BCD2BIN(cmos_read(0x0));
	now->tm_min = BCD2BIN(cmos_read(0x2));
	now->tm_hour = BCD2BIN(cmos_read(0x4));
	now->tm_mday = BCD2BIN(cmos_read(0x7));
	now->tm_mon = BCD2BIN(cmos_read(0x8));
	now->tm_year = BCD2BIN(cmos_read(0x9));
	now->tm_mon--;
}

void arch_time_get(struct tm *now) {
	memset(now, 0, sizeof(struct tm));
	now->tm_sec = time_get_epoch();
}

time_t arch_time_get_epoch()
{
	struct tm *tm, _t;tm = &_t;
	get_timed(tm);
	tm->tm_year += 30;
	return ((((unsigned long long)
		  (tm->tm_year/4 - tm->tm_year/100 + 
				tm->tm_year/400 + 367*tm->tm_mon/12 + tm->tm_mday) +
				tm->tm_year*365
	    )*24 + tm->tm_hour /* now have hours */
	  )*60 + tm->tm_min /* now have minutes */
	)*60 + tm->tm_sec; /* finally seconds */
}
