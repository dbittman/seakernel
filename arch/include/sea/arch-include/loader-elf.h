#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/loader/elf-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <sea/loader/elf-x86_64.h>
#endif

