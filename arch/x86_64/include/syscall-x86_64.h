#ifndef _SYSCALL_X86_64_H
#define _SYSCALL_X86_64_H
#define SC (long (*)(long, long, long, long, long))
#include <asm/system.h>
static long dosyscall(int num, long a, long b, long c, long d, long e)
{
	long x;
	asm("int $0x80":"=a"(x):"0" (num), "b" (a), "c" (b), "d" (c), "S" (d), "D" (e));
	return x;
}

#define __do_syscall_jump(ret, location, a, b, c, d, e) \
	ret = ((long (*)(long, long, long, long, long))location)(a, b, c, d, e)

#define SYSCALL_NUM_AND_RET regs->rax
#define _E_ regs->rdi
#define _D_ regs->rsi
#define _C_ regs->rdx
#define _B_ regs->rcx
#define _A_ regs->rbx

#endif
