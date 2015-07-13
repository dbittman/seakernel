#ifndef CONFIG_H
#define CONFIG_H

#include <sea_defines.h>

#include <sea/version.h>

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

#define KERN_PANIC 8
#define KERN_CRIT 7
#define KERN_ERROR 6
#define KERN_WARN 5
#define KERN_MILE 4
#define KERN_INFO 3
#define KERN_MSG 2
#define KERN_DEBUG 1
#define KERN_EVERY 0

/* MM */
#define STACK_SIZE (CONFIG_STACK_PAGES * 0x1000)

/* dev, fs */
#define NUM_CACHES 128

#define PIPE_SIZE 0x8000

/* video */
#define MAX_CONSOLES 10

#if CONFIG_DEBUG
#define EXEC_LOG 1
#endif

extern int PRINT_LEVEL;
long sys_sysconf(int cmd);
#include <sea/types.h>
int sys_gethostname(char *buf, size_t len);

#define VOID_CALL(...)

#define TRACE VOID_CALL

#endif
