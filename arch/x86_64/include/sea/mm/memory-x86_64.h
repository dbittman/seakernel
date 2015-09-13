#ifndef _MEMORY_X86_64_H
#define _MEMORY_X86_64_H

#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/cpu/registers.h>
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

/* SEE TODO IN syscall-x86_64.h, and then fix this location */
#define MMF_BEGIN                 0x0000000008000000
#define MMF_END                   0x0000000009000000

#define TOP_USER_HEAP             0x0000600000000000

#define EXEC_MINIMUM	                  0x400000

#define PHYS_PAGE_MAP             0xFFFFFF8000000000

/* where the signal injector code goes */
#define SIGNAL_INJECT                  (EXEC_MINIMUM-0x1000)

#define IS_KERN_MEM(x) (x > TOP_TASK_MEM)
#define IS_THREAD_SHARED_MEM(x) (!IS_KERN_MEM(x))

#define PAGE_SIZE_ORDER_MAX 1
static inline size_t mm_page_size(int order)
{
	static size_t __sizes[2] = { 0x1000, 0x200000 };
	assertmsg(order <= 1, "invalid page size order %d\n", order);
	return __sizes[order];
}

static inline size_t mm_page_size_closest(size_t length)
{
	if(length > 0x1000)
		return 0x200000;
	return 0x1000;
}

#define ATTRIB_MASK     0x8000000000000FFF
#define PAGE_PRESENT    0x1
#define PAGE_WRITE      0x2
#define PAGE_USER       0x4
#define PAGE_WRITECACHE 0x8
#define PAGE_NOCACHE    0x10
/*
 * WARNING: Features that use PAGE_LINK must be VERY CAREFUL to mm_vm_unmap_only that
 * page BEFORE the address space is freed normally, since that function DOES NOT KNOW
 * that multiple mappings may use that physical page! This can lead to memory leaks
 * and/or duplicate pages in the page stack!!! 
 */
#define PAGE_LINK      (1 << 10)
#define PAGE_LARGE     (1 << 7)
#define PAGE_SIZE 	   0x1000

void arch_mm_page_fault_handle(registers_t *regs, int, int);
typedef addr_t page_dir_t, page_table_t, pml4_t, pdpt_t;

#define PAGE_MASK          0xFFFFFFFFFFFFF000 /* TODO: fix this / PAGE_MASK_PHYSICAL */
#define PAGE_MASK_PHYSICAL 0x000FFFFFFFFFF000

#define PML4_INDEX(v) ((v >> 39) & 0x1FF)
#define PDPT_INDEX(v) ((v >> 30) & 0x1FF)
#define PD_INDEX(v) ((v >> 21) & 0x1FF)
#define PT_INDEX(v) ((v >> 12) & 0x1FF)

#endif
