#ifndef __SEA_ARCH_X86_COMMON_X86MSR_H
#define __SEA_ARCH_X86_COMMON_X86MSR_H

#include <sea/types.h>

static uint64_t read_msr(uint32_t msr)
{
	uint32_t r1, r2;
	__asm__ __volatile__("rdmsr":"=a"(r1), "=d"(r2):"c"(msr));
	return (((uint64_t)r2) << 32) | r1;
}

static void write_msr(uint32_t msr, uint64_t value)
{
   __asm__ __volatile__ ("wrmsr"::"a"((uint32_t)(value & 0xFFFFFFFF)),"d"((uint32_t)(value >> 32)),"c"(msr));
}

#define MSR_IA32_FEATURE_CONTROL 0x3A

#define MSR_IA32_PROCBASED_CTLS  0x482
#define MSR_IA32_PROCBASED_CTLS2 0x48B
#define MSR_IA32_VMX_EPT_VPID_CAP 0x48C
#define MSR_IA32_VMX_BASIC 0x480

#define MSR_IA32_SYSENTER_CS            0x00000174
#define MSR_IA32_SYSENTER_ESP           0x00000175
#define MSR_IA32_SYSENTER_EIP           0x00000176

#endif

