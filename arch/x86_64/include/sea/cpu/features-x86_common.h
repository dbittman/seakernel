#ifndef __ARCH_SEA_CPU_FEATURES_X86_COMMON_H
#define __ARCH_SEA_CPU_FEATURES_X86_COMMON_H

#include <sea/cpu/processor.h>

void x86_cpu_init_sse(struct cpu *me);
void x86_cpu_init_fpu(struct cpu *me);

#endif
