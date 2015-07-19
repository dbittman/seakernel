#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>
#include <sea/asm/system.h>
#include <sea/tm/timing.h>
#include <sea/vsprintf.h>
static int current_hz=1000;

int tm_get_current_frequency(void)
{
	return current_hz;
}

void tm_set_current_frequency_indicator(int hz)
{
	current_hz = hz;
}

time_t tm_timing_get_microseconds(void)
{
	return __current_cpu->ticker.tick;
}

void tm_timer_handler(registers_t *r, int int_no, int flags)
{
	if(current_thread) {
		ticker_tick(&current_thread->cpu->ticker, ONE_SECOND / current_hz);
		if(current_thread->system)
			add_atomic(&current_process->stime, ONE_SECOND / current_hz);
		else
			add_atomic(&current_process->utime, ONE_SECOND / current_hz);
		//current_thread->timeslice /= 2;
		//if(!current_thread->timeslice) {
			current_thread->flags |= THREAD_SCHEDULE;
		//	current_thread->timeslice = current_thread->priority;
		//}
	}
}

