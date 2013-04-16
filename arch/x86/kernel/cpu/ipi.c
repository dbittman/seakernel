/* Routines for generating and handling Interprocessor Interrupts
 * 
 * A CPU may send an IPI for the following reasons (in order of priority):
 *   1) kernel panic
 *   2) kernel debug interrupt
 *   3) update TLB
 *   4) kernel shutdown
 *   5) reshedule a new thread
 */
#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps.h>
mutex_t ipi_mutex;
int send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v)
{
	int to, send_status;
	int old = set_int(0);
	mutex_acquire(&ipi_mutex);
	IMPS_LAPIC_WRITE(LAPIC_ICR+0x10, (dst << 24));
	unsigned lower = v | (dest_shorthand << 18);
	IMPS_LAPIC_WRITE(LAPIC_ICR, v);

	/* Wait for send to finish */
	to = 0;
	do {
		asm("pause");
		send_status = IMPS_LAPIC_READ(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
	} while (send_status && (to++ < 1000));
	mutex_release(&ipi_mutex);
	set_int(old);
	return (to < 1000);
}
#endif
