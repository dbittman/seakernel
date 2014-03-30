#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/types-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <sea/types-x86_64.h>
#endif
