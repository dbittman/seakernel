#ifndef CONFIG_H
#define CONFIG_H

#include <../sea_defines.h>

#define TYPE_ARCH_X86 1
#define TYPE_ARCH_X86_64 2

#if CONFIG_ARCH == TYPE_ARCH_X86
	#define STACK_ELEMENT_SIZE 4
	#define BITS_PER_LONG 32
	#define LOCK_PREFIX "lock "
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
	#define STACK_ELEMENT_SIZE 8
	#define BITS_PER_LONG 64
	#define LOCK_PREFIX "lock "
#else
	#error "unsupported architecture"
#endif

#define DEF_PRINT_LEVEL CONFIG_LOG_LEVEL
#define MAX_TASKS (CONFIG_MAX_TASKS)

#define KMALLOC_INIT slab_init
#define KMALLOC_ALLOC do_kmalloc_slab
#define KMALLOC_FREE  do_kfree_slab
#define KMALLOC_NAME ((char *)"slab")

/* MM */
#define MAX_NODES 4096*4
//#define SLAB_DEBUG
#define RANGE_MUL 1.4
#define STACK_LOCATION (0xB0002000 + ((CONFIG_STACK_PAGES+1) * 0x1000)*2)
#define STACK_SIZE (CONFIG_STACK_PAGES * 0x1000)

//#define CONFIG_SMP 1

#define OOM_KILL 1
#define OOM_SLEEP 2
#define OOM_HANDLER CONFIG_OOM_HANDLER

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

#define BASE_SLAB_SIZE 8 /* In number of pages */
#define MAX_OBJ_ID 1024
//#define SWAP_DEBUG 1

/* dev, fs */
#define NUM_CACHES 128
#define NUM_TREES 1024
#define CACHE_CAP 20000

#define NUM_DT 4
#define PIPE_SIZE 0x4000
#define USE_CACHE 1
#define CACHE_READ 1

/* video */
#define MAX_CONSOLES 10

/* task */
#define SCHED_TTY CONFIG_SCHED_TTY
#define SCHED_TTY_CYC CONFIG_SCHED_TTY_AMOUNT

#define MAJ_VER 0
#define MIN_VER 2
#define PRE_VER 4

#define KVERSION (MAJ_VER * 200 + MIN_VER * 20 + PRE_VER)

#if PRE_VER < 8
#undef DEBUG
#define DEBUG 1 
#endif

#if CONFIG_DEBUG
#define EXEC_LOG 1
#else
#define EXEC_LOG 0
#endif

#ifdef DEBUG
#undef OOM_HANDLER
#define OOM_HANDLER 0
#endif

extern int PRINT_LEVEL;

long sys_sysconf(int cmd);
#endif
