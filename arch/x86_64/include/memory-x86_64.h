#ifndef _MEMORY_X86_64_H
#define _MEMORY_X86_64_H

#define PAGE_MASK      0xFFFFF000
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

#define PAGE_DIR_IDX(x) ((uint32_t)x/1024)
#define PAGE_TABLE_IDX(x) ((uint32_t)x%1024)
#define PAGE_DIR_PHYS(x) (x[1023]&PAGE_MASK)

#define disable_paging() \
__asm__ volatile ("mov %%cr0, %0" : "=r" (cr0temp)); \
cr0temp &= ~0x80000000; \
__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0temp));

#define enable_paging() \
__asm__ volatile ("mov %%cr0, %0" : "=r" (cr0temp)); \
cr0temp |= 0x80000000; \
__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0temp));

#define GET_PDIR_INFO(x) (page_dir_info *)(t_page + x*sizeof(page_dir_info))
/*
#define flush_pd() \
__asm__ __volatile__("mov %%cr3,%%rax\n\tmov %%rax,%%cr3": : :"ax", "eax", "rax")
*/

#define current_task ((kernel_state_flags&KSF_MMU) ? ((task_t *)(addr_t)page_directory[PAGE_DIR_IDX(SMP_CUR_TASK/PAGE_SIZE)]) : 0)

#endif
