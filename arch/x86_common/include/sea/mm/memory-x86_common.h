#ifndef __MEMORY_X86_COMMON
#define __MEMORY_X86_COMMON

#define ATTRIB_MASK    0x00000FFF
#define PAGE_PRESENT   0x1
#define PAGE_WRITE     0x2
#define PAGE_USER      0x4
#define PAGE_WRITECACHE 0x8
#define PAGE_NOCACHE   0x10
#define PAGE_COW       512
/*
 * WARNING: Features that use PAGE_LINK must be VERY CAREFUL to mm_vm_unmap_only that
 * page BEFORE the address space is freed normally, since that function DOES NOT KNOW
 * that multiple mappings may use that physical page! This can lead to memory leaks
 * and/or duplicate pages in the page stack!!! 
 */
#define PAGE_LINK      (1 << 10)
#define PAGE_SIZE 	   0x1000

#define MAP_NOIPI     0x8
#define MAP_PDLOCKED  0x4
#define MAP_NOCLEAR   0x2
#define MAP_CRIT      0x1
#define MAP_NORM      0x0

#include <sea/cpu/registers.h>
void arch_mm_page_fault_handle(registers_t *regs, int, int);
typedef addr_t page_dir_t, page_table_t, pml4_t, pdpt_t;
#endif
