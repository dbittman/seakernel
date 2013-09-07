#ifndef _ASM_SYSTEM_H
#define _ASM_SYSTEM_H

#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <asm/system-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <asm/system-x86_common.h>
#endif

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#endif
