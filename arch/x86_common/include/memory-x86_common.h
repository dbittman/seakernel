#ifndef __MEMORY_X86_COMMON
#define __MEMORY_X86_COMMON

#define ATTRIB_MASK    0x00000FFF
#define PAGE_PRESENT   0x1
#define PAGE_WRITE     0x2
#define PAGE_USER      0x4
#define PAGE_WRITECACHE 0x8
#define PAGE_NOCACHE   0x10
#define PAGE_COW       512
#define PAGE_SIZE 	   0x1000

#define MAP_NOIPI     0x8
#define MAP_PDLOCKED  0x4
#define MAP_NOCLEAR   0x2
#define MAP_CRIT      0x1
#define MAP_NORM      0x0

#include <sea/cpu/registers.h>
void arch_mm_page_fault(registers_t *regs);

#endif
