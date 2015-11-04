#include <sea/asm/system.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/timing.h>
#include <sea/vsprintf.h>
#include <stdatomic.h>
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

void tm_timer_handler(struct registers *r, int int_no, int flags)
{
	if(current_thread) {
		ticker_tick(&current_thread->cpu->ticker, ONE_SECOND / current_hz);
		if(current_thread->system)
			atomic_fetch_add_explicit(&current_process->stime,
					ONE_SECOND / current_hz, memory_order_relaxed);
		else
			atomic_fetch_add_explicit(&current_process->utime,
					ONE_SECOND / current_hz, memory_order_relaxed);
		current_thread->timeslice /= 2;
		if(!current_thread->timeslice) {
			current_thread->flags |= THREAD_SCHEDULE;
			current_thread->timeslice = current_thread->priority;
		}
	}
}

int sys_times(struct tms *buf)
{
	if(buf) {
		buf->tms_utime = current_process->utime * current_hz / ONE_SECOND;
		buf->tms_stime = current_process->stime * current_hz / ONE_SECOND;
		buf->tms_cstime = current_process->cstime * current_hz / ONE_SECOND;
		buf->tms_cutime = current_process->cutime * current_hz / ONE_SECOND;
	}
	return tm_timing_get_microseconds() * current_hz / ONE_SECOND;
}

