#ifndef CONFIG_H
#define CONFIG_H

#include <../sea_defines.h>

#include <sea/version.h>

#if CONFIG_DEBUG
#define DEBUG 1
#endif

#define TYPE_ARCH_X86 1
#define TYPE_ARCH_X86_64 2

#if CONFIG_ARCH == TYPE_ARCH_X86
	#define STACK_ELEMENT_SIZE 4
	#define BITS_PER_LONG 32
	#define LOCK_PREFIX "lock "
	#define ADDR_BITS 32
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
	#define STACK_ELEMENT_SIZE 8
	#define BITS_PER_LONG 64
	#define LOCK_PREFIX "lock "
	#define ADDR_BITS 64
#else
	#error "unsupported architecture"
#endif

#define DEF_PRINT_LEVEL CONFIG_LOG_LEVEL
#define MAX_TASKS (CONFIG_MAX_TASKS)

#define KMALLOC_INIT __mm_slab_init
#define KMALLOC_ALLOC __mm_do_kmalloc_slab
#define KMALLOC_FREE  __mm_do_kfree_slab
#define KMALLOC_NAME ((char *)"slab")

/* MM */
#define STACK_SIZE (CONFIG_STACK_PAGES * 0x1000)

#define BASE_SLAB_SIZE 8 /* In number of pages */
#define MAX_OBJ_ID 1024
//#define SWAP_DEBUG 1

/* dev, fs */
#define NUM_CACHES 128
#define NUM_TREES 1024
#define CACHE_CAP 20000

#define NUM_DT 4
#define PIPE_SIZE 0x8000

/* video */
#define MAX_CONSOLES 10

/* task */
#define SCHED_TTY CONFIG_SCHED_TTY
#define SCHED_TTY_CYC CONFIG_SCHED_TTY_AMOUNT

#if PRE_VER < 8
#undef DEBUG
#define DEBUG 1 
#endif

#if CONFIG_DEBUG
#define EXEC_LOG 1
#else
/* This can be easily changed.... */
#define EXEC_LOG 1
#endif

#ifdef DEBUG
#undef OOM_HANDLER
#define OOM_HANDLER 0
#endif

extern int PRINT_LEVEL;
long sys_sysconf(int cmd);

#endif
