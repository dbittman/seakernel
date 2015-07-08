#ifndef __SEA_CPU_INTERRUPT_H
#define __SEA_CPU_INTERRUPT_H

#include <sea/arch-include/cpu-isr.h>
#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/interrupt-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/interrupt-x86_common.h>
#endif

#define MAX_HANDLERS 256
#define MAX_INTERRUPTS 256

int cpu_interrupt_set(unsigned _new);
void cpu_interrupt_set_flag(int flag);
int cpu_interrupt_get_flag();
int arch_cpu_interrupt_set(unsigned _new);
void arch_cpu_interrupt_set_flag(int flag);
int arch_cpu_interrupt_get_flag();

void cpu_interrupt_syscall_entry(registers_t *regs, int syscall_number);
void cpu_interrupt_isr_entry(registers_t *regs, int int_no, addr_t return_address);
void cpu_interrupt_irq_entry(registers_t *regs, int int_no);

void arch_cpu_timer_install(int hz);
void cpu_timer_install(int hz);

int cpu_interrupt_register_handler(int num, void (*fn)(int, int));
void cpu_interrupt_unregister_handler(u8int n, int id);

void interrupt_init();

extern volatile unsigned long interrupt_counts[256];

#if CONFIG_SMP
void cpu_handle_ipi_tlb(volatile registers_t);
void cpu_handle_ipi_tlb_ack(volatile registers_t);
void cpu_handle_ipi_reschedule(volatile registers_t);
void cpu_handle_ipi_halt(volatile registers_t);
#endif

#endif

