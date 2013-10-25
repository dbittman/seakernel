#ifndef _ASM_SYSTEM_H
#define _ASM_SYSTEM_H

#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <asm/system-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <asm/system-x86_common.h>
#endif

#include <types.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)


#define UPPER32(x) \
	((sizeof(x) > 32) ? ((x >> 32) & 0xFFFFFFFF) : 0)

#define ALIGN(x,a) \
		((void *)((((addr_t)x) & ~(a-1)) + a))

#endif
