#include <sea/config.h>

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/isr-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <sea/cpu/isr-x86_64.h>
#endif

#if CONFIG_ARCH == TYPE_ARCH_X86
  #include <sea/cpu/interrupt-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
  #include <sea/cpu/interrupt-x86_common.h>
#endif


