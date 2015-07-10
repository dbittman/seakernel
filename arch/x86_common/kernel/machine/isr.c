/* Privides functions for interrupt handling. */
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <sea/asm/system.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/fs/proc.h>
#include <sea/tm/schedule.h>
#include <sea/cpu/cpu-io.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif

/* don't need to worry about other processors getting in the way here, since
 * this is only used if SMP is disabled or unavailable */
static void ack_pic(int n)
{
	assert(interrupt_controller == IOINT_PIC);
	if(n >= IRQ0 && n < IRQ15) {
		if (n >= 40)
			outb(0xA0, 0x20);
		outb(0x20, 0x20);
	}
}

void arch_interrupt_ipi_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	cpu_interrupt_set(0);
	add_atomic(&interrupt_counts[regs.int_no], 1);
#if CONFIG_SMP
	/* delegate to the proper handler, in ipi.c */
	switch(regs.int_no) {
		case IPI_DEBUG:
		case IPI_SHUTDOWN:
		case IPI_PANIC:
			LAPIC_WRITE(LAPIC_TPR, 0xFFFFFFFF);
			cpu_handle_ipi_halt(regs);
			break;
		case IPI_SCHED:
			cpu_handle_ipi_reschedule(regs);
			break;
		case IPI_TLB:
			cpu_handle_ipi_tlb(regs);
			break;
		case IPI_TLB_ACK:
			cpu_handle_ipi_tlb_ack(regs);
			break;
		default:
			panic(PANIC_NOSYNC, "invalid interprocessor interrupt number: %d", regs.int_no);
	}
#endif
	cpu_interrupt_set(0);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* this should NEVER enter from an interrupt handler, 
 * and only from kernel code in the one case of calling
 * sys_setup() */
void arch_interrupt_syscall_handler(volatile registers_t regs)
{
	/* don't need to save the flag here, since it will always be true */
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(regs.int_no == 0x80 && ((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	cpu_interrupt_syscall_entry(&regs,
#if CONFIG_ARCH == TYPE_ARCH_X86_64
		regs.rax
#elif CONFIG_ARCH == TYPE_ARCH_X86
		regs.eax
#endif
	);
#if CONFIG_SMP
	lapic_eoi();
#endif

	if(current_thread->flags & TF_SCHED)
		tm_schedule();
}

/* This gets called from our ASM interrupt handler stub. */
void arch_interrupt_isr_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20));
#endif
	cpu_interrupt_isr_entry(&regs, regs.int_no, regs.eip);
	/* send out the EOI... */
#if CONFIG_SMP
	lapic_eoi();
#endif
	if(current_thread->flags & TF_SCHED)
		tm_schedule();
}

void arch_interrupt_irq_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	cpu_interrupt_irq_entry(&regs, regs.int_no);
	/* and send out the EOIs */
	if(interrupt_controller == IOINT_PIC) ack_pic(regs.int_no);
#if CONFIG_SMP
	lapic_eoi();
#endif
	/* TODO: put this in arch-indep code */
	if(current_thread->flags & TF_SCHED)
		tm_schedule();
}

void arch_interrupt_reset_timer_state(void)
{
	if(interrupt_controller == IOINT_PIC) ack_pic(32);
}

