#ifndef __SEA_CPU_INTERRUPT_H
#define __SEA_CPU_INTERRUPT_H

#include <sea/config.h>

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/isr-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <sea/cpu/isr-x86_64.h>
#endif

#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/interrupt-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/interrupt-x86_common.h>
#endif

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

extern volatile long int_count[256];

#endif