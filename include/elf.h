#ifndef __ELF_H
#define __ELF_H

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <elf-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <elf-x86_64.h>
#endif

#endif
