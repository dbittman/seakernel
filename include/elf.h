#ifndef __ELF_H
#define __ELF_H

#include <config.h>
#include <types.h>

typedef struct { 
	const char *name; 
	addr_t ptr; 
	int flag;
} kernel_symbol_t;

#if CONFIG_ARCH == TYPE_ARCH_X86
#include <elf-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <elf-x86_64.h>
#endif

#endif
