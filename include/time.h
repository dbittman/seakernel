#ifndef _TIME_H
#define _TIME_H
typedef int clock_t;
struct tm
{
  int	tm_sec;
  int	tm_min;
  int	tm_hour;
  int	tm_mday;
  int	tm_mon;
  int	tm_year;
  int	tm_wday;
  int	tm_yday;
  int	tm_isdst;
};
struct timeval {
  int      tv_sec;
  int tv_usec;
};
struct tms {
	clock_t tms_utime;
	clock_t tms_stime;
	clock_t tms_cutime;
	clock_t tms_cstime;
};

void get_time(struct tm *now);
unsigned long long get_epoch_time();
#endif
