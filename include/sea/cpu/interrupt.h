#ifndef __SEA_CPU_INTERRUPT_H
#define __SEA_CPU_INTERRUPT_H

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <isr-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <isr-x86_64.h>
#endif

#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/interrupt-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/interrupt-x86_common.h>
#endif

int interrupt_set(unsigned _new);
void interrupt_set_flag(int flag);
int interrupt_get_flag();

#endif
