#include <sea/config.h>

#define current_tss (&((cpu_t *)current_task->cpu)->tss)

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <context-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <context-x86_64.h>

#endif
