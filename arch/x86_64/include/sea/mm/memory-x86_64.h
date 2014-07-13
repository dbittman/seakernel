#ifndef _MEMORY_X86_64_H
#define _MEMORY_X86_64_H

#include <sea/mm/memory-x86_common.h>
#include <sea/types.h>
/* due the the odd way that we are forced to address memory...
 * 
 * You can't use all 64 bits of an address, only the least significant 48.
 * In addition, all higher bits must be sign extended from bit 47. This gives
 * us two ranges of virtual memory:
 * 0x0000000000000000 - 0x00007FFFFFFFFFFF
 * 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
 */
#define TOP_TASK_MEM              0x00007FFFFFFFFFFF
#define TOP_TASK_MEM_EXEC         0x0000700000000000

#define MMF_BEGIN                 0x0000600000000000
#define MMF_END                   0x0000700000000000

#define TOP_USER_HEAP             0x0000600000000000
#define TOP_LOWER_KERNEL                  0x40000000

#define STACK_LOCATION     (0x0000700000002000 + ((CONFIG_STACK_PAGES+1) * 0x1000)*2)

#define BOTTOM_HIGHER_KERNEL      0xFFFF820000000000

#define EXEC_MINIMUM	                  0x50000000

#define START_FREE_LOCATION               0x40000000

#define KMALLOC_ADDR_START        0xFFFF820000000000
#define KMALLOC_ADDR_END          0xFFFF830000000000

#define CONTIGUOUS_VIRT_START     0xFFFFFF7FB8000000
#define CONTIGUOUS_VIRT_END       0xFFFFFF7FC0000000

#define DEVICE_MAP_END            0xFFFFFF7FB8000000

#define DEVICE_MAP_START          0xFFFFFF0000000000

#define PM_STACK_ADDR             0xFFFFFF7FC0000000
#define PM_STACK_ADDR_TOP         0xFFFFFF8000000000

#define PHYS_PAGE_MAP             0xFFFFFF8000000000

#define PDIR_INFO_START           0xFFFF800000000000

#define PDIR_DATA		          0xFFFF800000000000

#define PHYSICAL_PML4_INDEX       0xFFFFFE8000000000

#define RESERVED1                 0xFFFFFE8000000000
#define RESERVED1_END             0xFFFFFF0000000000

#define CURRENT_TASK_POINTER      0xFFFF810000000000

/* where the signal injector code goes */
#define SIGNAL_INJECT                  (0x40000000-0x1000)

#define IS_KERN_MEM(x) (x < TOP_LOWER_KERNEL || x > TOP_TASK_MEM)

#define IS_THREAD_SHARED_MEM(x) (((!(x >= TOP_TASK_MEM_EXEC && x < TOP_TASK_MEM)) || ((x&PAGE_MASK) == PDIR_DATA)))

#define PAGE_MASK      0xFFFFFFFFFFFFF000
#define PAGE_LARGE (1 << 7)
#define PML4_IDX(x) ((x/0x8000000) % 512)
#define PDPT_IDX(x) ((x / 0x40000) % 512)
#define PAGE_DIR_IDX(x) ((x / 0x200) % 512)
#define PAGE_TABLE_IDX(x) (x % 512)

#define PAGE_SIZE_LOWER_KERNEL (2 * 1024 * 1024)

#define flush_pd() \
__asm__ __volatile__("mov %%cr3,%%rax\n\tmov %%rax,%%cr3": : :"ax", "eax", "rax")

#define current_task ((volatile task_t *)(kernel_task ? ((volatile task_t *)(*((volatile addr_t *)CURRENT_TASK_POINTER))) : 0))
addr_t arch_mm_alloc_physical_page_zero();
addr_t get_next_mm_device_page();
int vm_early_map(addr_t *, addr_t virt, addr_t phys, unsigned attr, unsigned opt);
extern addr_t *kernel_dir_phys;
#endif
