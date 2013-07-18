#ifndef _MEMORY_X86_64_H
#define _MEMORY_X86_64_H
/* due the the odd way that we are forced to address memory...
 * 
 * You can't use all 64 bits of an address, only the least significant 48.
 * In addition, all higher bits must be sign extended from bit 47. This gives
 * us two ranges of memory:
 * 0x0000000000000000 - 0x00007FFFFFFFFFFF
 * 0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
 */
#define TOP_TASK_MEM       0x00007FFFFFFFFFFF
#define TOP_TASK_MEM_EXEC  0x0000700000000000
#define TOP_USER_HEAP      0x0000700000000000
#define TOP_LOWER_KERNEL   0x100000000

#define EXEC_MINIMUM	   0x300000000

#define SOLIB_RELOC_START  0x200000000
#define SOLIB_RELOC_END    0x300000000

#define KMALLOC_ADDR_START 0x100000000
#define KMALLOC_ADDR_END   0x200000000

#define PM_STACK_ADDR      0xFFFFFF7FC0000000
#define PM_STACK_ADDR_TOP  0xFFFFFF8000000000

#define PHYS_PAGE_MAP      0xFFFFFF8000000000

#define PDIR_INFO_START    0xF0000000

#define PDIR_DATA		   0xF0000000

/* this entry in the page directory actually just points to the current
 * task */
#define SMP_CUR_TASK       0xF0400000

#define MMF_SHARED_START   0xB8000000
#define MMF_SHARED_END     0xC0000000
#define MMF_PRIV_START     0xA0000000
#define MMF_PRIV_END       0xB0000000

/* where the signal injector code goes */
#define SIGNAL_INJECT      0xB0001000

#define IS_KERN_MEM(x) (x < TOP_LOWER_KERNEL || (x > MMF_SHARED_START && x < PDIR_DATA))

#define IS_THREAD_SHARED_MEM(x) (((!(x >= TOP_TASK_MEM_EXEC && x < TOP_TASK_MEM)) || ((x&PAGE_MASK) == PDIR_DATA)) && x < DIR_PHYS)

#define page_directory ((void *)0)
#define page_tables ((addr_t *)0)

#define PAGE_MASK      0xFFFFFFFFFFFFF000
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

#define PML4_IDX(x) ((x/0x8000000) % 512)
#define PDPT_IDX(x) ((x / 0x40000) % 512)
#define PAGE_DIR_IDX(x) ((x / 0x200) % 512)
#define PAGE_TABLE_IDX(x) (x % 512)

#define flush_pd() \
__asm__ __volatile__("mov %%cr3,%%rax\n\tmov %%rax,%%cr3": : :"ax", "eax", "rax")

#define current_task ((kernel_state_flags&KSF_MMU) ? ((task_t *)(addr_t)0) : 0)

#endif
