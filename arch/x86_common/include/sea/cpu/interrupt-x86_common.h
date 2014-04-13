#ifndef __ARCH_SEA_CPU_INTERRUPT_X86_COMMON_H
#define __ARCH_SEA_CPU_INTERRUPT_X86_COMMON_H

#include <sea/cpu/registers.h>

int interrupt_register_handler(u8int num, isr_t stage1_handler, isr_t stage2_handler);
void interrupt_unregister_handler(u8int n, int id);

void arch_interrupt_ipi_handler(volatile registers_t regs);
void arch_interrupt_syscall_handler(volatile registers_t regs);
void arch_interrupt_isr_handler(volatile registers_t regs);
void arch_interrupt_irq_handler(volatile registers_t regs);
void arch_interrupt_reset_timer_state();
void __KT_try_handle_stage2_interrupts();
void interrupt_init();

#define arch_interrupt_enable()  asm("sti")
#define arch_interrupt_disable() asm("cli")

void x86_cpu_handle_ipi_cpu_halt(volatile registers_t regs);
void x86_cpu_handle_ipi_reschedule(volatile registers_t regs);
void x86_cpu_handle_ipi_tlb(volatile registers_t regs);
void x86_cpu_handle_ipi_tlb_ack(volatile registers_t regs);
int x86_cpu_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v);

#endif
