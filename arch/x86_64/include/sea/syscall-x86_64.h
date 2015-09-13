#ifndef _SYSCALL_X86_64_H
#define _SYSCALL_X86_64_H
#define SC (long (*)(long, long, long, long, long))
#include <sea/asm/system.h>
static long dosyscall(int num, long a, long b, long c, long d, long e)
{
	long x;
	__asm__ __volatile__("int $0x80":"=a"(x):"0" (num), "b" (a), "c" (b), "d" (c), "S" (d), "D" (e));
	return x;
}

/* backwards because of the calling conventions...it works. */
/* TODO: here we're returing an int, and casting it to a long, since most
 * syscalls return an int. Make this every syscall. syscalls only return
 * error values, any other return value must be through pointer parameters */
#define __do_syscall_jump(ret, location, a, b, c, d, e) \
	ret = (long)((int (*)(long, long, long, long, long))location)(e, d, c, b, a)

#define SYSCALL_NUM_AND_RET regs->rax
#define _E_ regs->rdi
#define _D_ regs->rsi
#define _C_ regs->rdx
#define _B_ regs->rcx
#define _A_ regs->rbx

#define MEMMAP_SYSGATE_ADDRESS_SIZE 10
static unsigned char signal_return_injector[MEMMAP_SYSGATE_ADDRESS_SIZE] = {
	0x48,
	0x31, /* xor rax, rax */
	0xc0,
	
	0xB8,
	0x80,
	0x00, /* mov eax, 128 */
	0x00,
	0x00,
	
	0xCD, /* int 0x80 */
	0x80
};

#endif

