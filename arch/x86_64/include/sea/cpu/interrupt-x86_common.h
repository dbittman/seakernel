#ifndef __ARCH_SEA_CPU_INTERRUPT_X86_COMMON_H
#define __ARCH_SEA_CPU_INTERRUPT_X86_COMMON_H

#include <sea/cpu/registers.h>
#include <sea/asm/system.h>

void arch_interrupt_ipi_handler(volatile struct registers regs);
void arch_interrupt_syscall_handler(volatile struct registers regs);
void arch_interrupt_isr_handler(volatile struct registers regs);
void arch_interrupt_irq_handler(volatile struct registers regs);
void arch_interrupt_reset_timer_state();

#define arch_interrupt_enable()  __asm__ __volatile__ ("sti")
#define arch_interrupt_disable() __asm__ __volatile__ ("cli")

void x86_cpu_handle_ipi_cpu_halt(volatile struct registers regs);
void x86_cpu_handle_ipi_reschedule(volatile struct registers regs);
void x86_cpu_handle_ipi_tlb(volatile struct registers regs);
void x86_cpu_handle_ipi_tlb_ack(volatile struct registers regs);
int x86_cpu_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v);

#endif
