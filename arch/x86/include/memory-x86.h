#ifndef _MEMORY_X86_H
#define _MEMORY_X86_H

#define TOP_TASK_MEM       0xB8000000
#define TOP_TASK_MEM_EXEC  0xB0000000
#define TOP_USER_HEAP      0xA0000000
#define TOP_LOWER_KERNEL   0x10000000

#define EXEC_MINIMUM	   0x30000000

#define SOLIB_RELOC_START  0x10000000
#define SOLIB_RELOC_END    0x20000000

#define KMALLOC_ADDR_START 0xC0000000
#define KMALLOC_ADDR_END   0xE0000000

#define PM_STACK_ADDR      0xE0000000
#define PM_STACK_ADDR_TOP  0xF0000000

#define PDIR_INFO_START    0xF0000000

#define PDIR_DATA		   0xF0000000

/* this entry in the page directory actually just points to the current
 * task */
#define SMP_CUR_TASK       0xF0400000

#define DIR_PHYS           0xFFBFF000
#define TBL_PHYS           0xFFC00000

#define MMF_SHARED_START   0xB8000000
#define MMF_SHARED_END     0xC0000000
#define MMF_PRIV_START     0xA0000000
#define MMF_PRIV_END       0xB0000000

/* where the signal injector code goes */
#define SIGNAL_INJECT      0xB0001000

#define VIRT_TEMP (0xB0000000)

#define IS_KERN_MEM(x) (x < TOP_LOWER_KERNEL || (x > MMF_SHARED_START && x < PDIR_DATA))

#define IS_THREAD_SHARED_MEM(x) (((!(x >= TOP_TASK_MEM_EXEC && x < TOP_TASK_MEM)) || ((x&PAGE_MASK) == PDIR_DATA)) && x < DIR_PHYS)
#define page_directory ((unsigned *)DIR_PHYS)
#define page_tables ((unsigned *)TBL_PHYS)

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

#define flush_pd() \
 __asm__ __volatile__("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3": : :"ax", "eax")

#define current_task ((kernel_state_flags&KSF_MMU) ? ((task_t *)page_directory[PAGE_DIR_IDX(SMP_CUR_TASK/PAGE_SIZE)]) : 0)
 
#endif
