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

#define MEMMAP_KERNEL_START       0xFFFF800000000000

#define KMALLOC_ADDR_START        0xFFFF801000000000
#define KMALLOC_ADDR_END          0xFFFF802000000000


#define VIRTPAGES_START           0xFFFF803000000000
#define VIRTPAGES_END             0xFFFF804000000000

#define CONTIGUOUS_VIRT_START     0xFFFF804000000000
#define CONTIGUOUS_VIRT_END       0xFFFF805000000000

#define DEVICE_MAP_START          0xFFFF805000000000
#define DEVICE_MAP_END            0xFFFF806000000000


#define KERNELMODE_STACKS_START   0x0000702000000000
#define KERNELMODE_STACKS_END     0x0000703000000000
#define USERMODE_STACKS_START     0x0000600010000000
#define USERMODE_STACKS_END       0x0000600100000000






#define TOP_TASK_MEM              0x00006FFFFFFFFFFF
#define TOP_TASK_MEM_EXEC         0x0000600000000000

#define MMF_BEGIN                 0x0000600000000000
#define MMF_END                   0x0000600010000000

#define TOP_USER_HEAP             0x0000600000000000
#define TOP_LOWER_KERNEL                  0x40000000


#define BOTTOM_HIGHER_KERNEL      0xFFFF820000000000

#define EXEC_MINIMUM	                  0x400000

#define START_FREE_LOCATION               0x400000

#define PM_STACK_ADDR             0xFFFFFF7FC0000000
#define PM_STACK_ADDR_TOP         0xFFFFFF8000000000

#define PHYS_PAGE_MAP             0xFFFFFF8000000000

#define PHYSICAL_PML4_INDEX       0xFFFFFE8000000000

#define RESERVED1                 0xFFFFFE8000000000
#define RESERVED1_END             0xFFFFFF0000000000

/* where the signal injector code goes */
#define SIGNAL_INJECT                  (EXEC_MINIMUM-0x1000)

#define IS_KERN_MEM(x) (x > TOP_TASK_MEM)

#define IS_THREAD_SHARED_MEM(x) (!IS_KERN_MEM(x))

#define PAGE_MASK      0xFFFFFFFFFFFFF000 /* TODO: fix this / PAGE_MASK_PHYSICAL */
#define PAGE_LARGE (1 << 7)
#define PML4_IDX(x) ((x/0x8000000) % 512)
#define PDPT_IDX(x) ((x / 0x40000) % 512)
#define PAGE_DIR_IDX(x) ((x / 0x200) % 512)
#define PAGE_TABLE_IDX(x) (x % 512)

#define PML4_INDEX(v) ((v >> 39) & 0x1FF)
#define PDPT_INDEX(v) ((v >> 30) & 0x1FF)
#define PD_INDEX(v) ((v >> 21) & 0x1FF)
#define PT_INDEX(v) ((v >> 12) & 0x1FF)

#define PAGE_MASK_PHYSICAL      0xFFFFFFFFFF000

#define PAGE_SIZE_LOWER_KERNEL (2 * 1024 * 1024)

#define flush_pd() \
__asm__ __volatile__("mov %%cr3,%%rax\n\tmov %%rax,%%cr3": : :"ax", "eax", "rax")

addr_t arch_mm_alloc_physical_page_zero();
int vm_early_map(addr_t *, addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int arch_mm_vm_early_map(pml4_t *pml4, addr_t virt, addr_t phys, unsigned attr, unsigned opt);
extern addr_t *kernel_dir_phys;
#endif
