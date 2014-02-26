#ifndef __SEA_CPU_REGISTERS_H
#define __SEA_CPU_REGISTERS_H
#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/registers-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <sea/cpu/registers-x86_64.h>
#endif


#endif
