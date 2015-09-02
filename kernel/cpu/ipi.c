#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <sea/syscall.h>

#if CONFIG_SMP
void cpu_send_ipi(int dest, unsigned signal, unsigned flags)
{
	arch_cpu_send_ipi(dest, signal, flags);
}

/* note! cpu_get_interrupt_flag lies here! it reports what the
 * interrupt state WILL BE when these return. interrupts are
 * indeed disabled */
void cpu_handle_ipi_tlb(registers_t *regs)
{
	mm_flush_page_tables();
}

void cpu_handle_ipi_tlb_ack(registers_t *regs)
{
	
}

void cpu_handle_ipi_reschedule(registers_t *regs)
{
	/* we don't allow a reschedule request if this cpu has
	 * interrupts disabled */
	if(!cpu_interrupt_get_flag())
		return;
	tm_schedule();
}

void cpu_handle_ipi_halt(registers_t *regs)
{
	atomic_fetch_add(&num_halted_cpus, 1);
	cpu_halt();
}

#endif
