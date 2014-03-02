#ifndef __ARCH_SEA_CPU_FEATURES_X86_COMMON_H
#define __ARCH_SEA_CPU_FEATURES_X86_COMMON_H

#include <sea/cpu/processor.h>

void x86_cpu_init_sse(cpu_t *me);
void x86_cpu_init_fpu(cpu_t *me);

#endif
