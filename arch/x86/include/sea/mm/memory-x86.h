#ifndef _MEMORY_X86_H
#define _MEMORY_X86_H

#include <sea/kernel.h>
#include <sea/mm/memory-x86_common.h>
#include <sea/types.h>
/* TODO: unfuck the memory maps */
#define TOP_TASK_MEM       0xA8000000
#define TOP_TASK_MEM_EXEC  0xA0000000
#define USERMODE_STACKS_START 0xA1000000
#define USERMODE_STACKS_END   0xA8000000
#define TOP_USER_HEAP      0x90000000
#define TOP_LOWER_KERNEL   0x10000000

#define EXEC_MINIMUM	   0x30000000

#define MMF_BEGIN          0x30000000
#define MMF_END            0x40000000

#define SOLIB_RELOC_START  0x10000000
#define SOLIB_RELOC_END    0x20000000

#define KMALLOC_ADDR_START 0xC0000000
#define KMALLOC_ADDR_END   0xE0000000

#define PM_STACK_ADDR      0xE0000000
#define PM_STACK_ADDR_TOP  0xF0000000

#define PDIR_INFO_START    0xF0000000

#define DIR_PHYS           0xFFBFF000
#define TBL_PHYS           0xFFC00000

#define DEVICE_MAP_START   0xA8000000
#define DEVICE_MAP_END     0xB0000000

#define CONTIGUOUS_VIRT_START 0xB0000000
#define CONTIGUOUS_VIRT_END   0xB8000000

/* where the signal injector code goes */
#define SIGNAL_INJECT      0xA0001000

#define VIRT_TEMP (0xA0000000)

/* TODO: need better defines for the memory map */
#define IS_KERN_MEM(x) (x < TOP_LOWER_KERNEL || (x >= DEVICE_MAP_START))

/* TODO: these are ugly anyway */
#define IS_THREAD_SHARED_MEM(x) (!IS_KERN_MEM(x))
#define page_directory ((unsigned *)DIR_PHYS)
#define page_tables ((unsigned *)TBL_PHYS)

#define PAGE_MASK      0xFFFFF000

#define PAGE_SIZE_LOWER_KERNEL 0x1000

#define PAGE_DIR_IDX(x) ((uint32_t)x/1024)
#define PAGE_TABLE_IDX(x) ((uint32_t)x%1024)
#define PAGE_DIR_PHYS(x) (x[1023]&PAGE_MASK)

/* TODO: check if we're doing this needlessly */
#define flush_pd() \
 __asm__ __volatile__("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3": : :"ax", "eax")

#endif
