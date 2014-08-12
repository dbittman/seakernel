/* every file should include this. This defines the basic things needed for
 * kernel operation. asm is redefined here, which is vital. */

#ifndef KERNEL_H
#define KERNEL_H

#include <sea/cpu/cpu-io.h>
#include <sea/string.h>
#include <sea/vsprintf.h>
#include <sea/tty/terminal.h>
#include <sea/mm/kmalloc.h>
#include <sea/syscall.h>
#include <sea/errno.h>

#define KSF_MMU            0x1
#define KSF_SHUTDOWN       0x2
#define KSF_PANICING       0x4
#if CONFIG_SMP
  #define KSF_CPUS_RUNNING 0x8
  #define KSF_SMP_ENABLE   0x10
#endif
#define KSF_PAGING         0x20
#define KSF_HAVEEXECED     0x40 /* have we exec'd once? we dont check for valid pointers if this is unset */
#define KSF_MEMMAPPED      0x80 /* is memory mapped? (used by pmm) */
extern volatile unsigned kernel_state_flags;
#define set_ksf(flag) or_atomic(&kernel_state_flags, flag)
#define unset_ksf(flag) and_atomic(&kernel_state_flags, ~flag)
extern volatile unsigned int __allow_idle;

#define PANIC_NOSYNC  1
#define PANIC_MEM     2
#define PANIC_VERBOSE 4

#if CONFIG_ENABLE_ASSERTS
  #define assert(c) \
	do {\
		if(__builtin_expect((!(c)),0)) \
			panic_assert(__FILE__, __LINE__, #c); \
	} while(0)
#else
  #define assert(c) {}
#endif

struct inode *kt_set_as_kernel_task(char *name);
void panic(int flags, char *fmt, ...);
void kernel_reset();
void panic_assert(const char *file, u32int line, const char *desc);
void kernel_poweroff();
extern int april_fools;
void do_reset();
int sys_gethostname(char *buf, size_t len);

#endif

