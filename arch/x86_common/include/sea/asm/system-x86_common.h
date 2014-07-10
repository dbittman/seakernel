#ifndef __ASM_SYSTEM_X86_64_H
#define __ASM_SYSTEM_X86_64_H
#define asm __sync_synchronize(); __asm__ __volatile__
#include <sea/types.h>
static uint64_t read_msr(uint32_t msr)
{
	uint32_t r1, r2;
	asm("rdmsr":"=a"(r1), "=d"(r2):"c"(msr));
	return (((uint64_t)r2) << 32) | r1;
}

static void write_msr(uint32_t msr, uint64_t value)
{
   asm ("wrmsr"::"a"((uint32_t)(value & 0xFFFFFFFF)),"d"((uint32_t)(value >> 32)),"c"(msr));
}

#define MSR_IA32_FEATURE_CONTROL 0x3A

#define MSR_IA32_PROCBASED_CTLS  0x482
#define MSR_IA32_PROCBASED_CTLS2 0x48B
#define MSR_IA32_VMX_EPT_VPID_CAP 0x48C
#define MSR_IA32_VMX_BASIC 0x480
#define arch_cpu_jump(x) asm("jmp *%0"::"r"(x))
#define arch_cpu_pause() asm("pause")
#define arch_cpu_halt() asm("hlt")

#define nop() __sync_synchronize();__asm__ __volatile__ ("nop")
int set_int(unsigned new);
extern char tables;

#define BS8(x) (x)
#define BS16(x) (((x&0xFF00)>>8)|((x&0x00FF)<<8))
#define BS32(x) (((x&0xFF000000)>>24)|((x&0x00FF0000)>>8)|((x&0x0000FF00)<<8)|((x&0x000000FF)<<24))
#define BS64(x) (x)

#define LITTLE_ENDIAN

#ifdef LITTLE_ENDIAN

#define LITTLE_TO_HOST8(x) (x)
#define LITTLE_TO_HOST16(x) (x)
#define LITTLE_TO_HOST32(x) (x)
#define LITTLE_TO_HOST64(x) (x)

#define HOST_TO_LITTLE8(x) (x)
#define HOST_TO_LITTLE16(x) (x)
#define HOST_TO_LITTLE32(x) (x)
#define HOST_TO_LITTLE64(x) (x)

#define BIG_TO_HOST8(x) BS8((x))
#define BIG_TO_HOST16(x) BS16((x))
#define BIG_TO_HOST32(x) BS32((x))
#define BIG_TO_HOST64(x) BS64((x))

#define HOST_TO_BIG8(x) BS8((x))
#define HOST_TO_BIG16(x) BS16((x))
#define HOST_TO_BIG32(x) BS32((x))
#define HOST_TO_BIG64(x) BS64((x))

#else // else Big endian

#define BIG_TO_HOST8(x) (x)
#define BIG_TO_HOST16(x) (x)
#define BIG_TO_HOST32(x) (x)
#define BIG_TO_HOST64(x) (x)

#define HOST_TO_BIG8(x) (x)
#define HOST_TO_BIG16(x) (x)
#define HOST_TO_BIG32(x) (x)
#define HOST_TO_BIG64(x) (x)

#define LITTLE_TO_HOST8(x) BS8((x))
#define LITTLE_TO_HOST16(x) BS16((x))
#define LITTLE_TO_HOST32(x) BS32((x))
#define LITTLE_TO_HOST64(x) BS64((x))

#define HOST_TO_LITTLE8(x) BS8((x))
#define HOST_TO_LITTLE16(x) BS16((x))
#define HOST_TO_LITTLE32(x) BS32((x))
#define HOST_TO_LITTLE64(x) BS64((x))

#endif

#endif
