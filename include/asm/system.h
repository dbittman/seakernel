#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <asm/system-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <asm/system-x86_64.h>
#endif
