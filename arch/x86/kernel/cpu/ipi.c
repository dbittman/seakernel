/* Routines for generating and handling Interprocessor Interrupts
 * 
 * A CPU may send an IPI for the following reasons (in order of priority):
 *   1) kernel panic
 *   2) kernel debug interrupt
 *   3) update TLB
 *   4) ack update TLB
 *   5) kernel shutdown
 *   6) reshedule a new thread
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
	assert((v & LAPIC_ICR_DM_INIT) || (v & LAPIC_ICR_LEVELASSERT));
	/* if we've initialized SMP, but we've disabled it, don't send any IPIs */
	if(!(kernel_state_flags & KSF_SMP_ENABLE) && (kernel_state_flags & KSF_CPUS_RUNNING))
		return 1;
	int to, send_status;
	int old = set_int(0);
	mutex_acquire(&ipi_mutex);
	/* Writing to the lower ICR register causes the interrupt
	 * to get sent off (Intel 3A 10.6.1), so do the higher reg first */
	IMPS_LAPIC_WRITE(LAPIC_ICR+0x10, (dst << 24));
	unsigned lower = v | (dest_shorthand << 18);
	/* gotta have assert for all except init */
	IMPS_LAPIC_WRITE(LAPIC_ICR, lower);
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

void handle_ipi_cpu_halt(volatile registers_t regs)
{
	set_int(0);
	/* No interrupts */
	IMPS_LAPIC_WRITE(LAPIC_TPR, 0xFFFFFFFF);
	asm("cli");
	while(1) asm("hlt");
}

void handle_ipi_reschedule(volatile registers_t regs)
{
	run_scheduler();
}

void handle_ipi_tlb(volatile registers_t regs)
{
	/* flush the TLB */
	flush_pd();
}

void handle_ipi_tlb_ack(volatile registers_t regs)
{
	
}

#endif
