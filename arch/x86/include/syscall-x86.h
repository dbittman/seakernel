#ifndef _SYSCALL_X86_H
#define _SYSCALL_X86_H
#define SC (int (*)(int, int, int, int, int))
int dosyscall(int num, int a, int b, int c, int d, int e);
#define __do_syscall_jump(ret, location, a, b, c, d, e) __asm__ __volatile__(" \
	push %1; \
	push %2; \
	push %3; \
	push %4; \
	push %5; \
	call *%6; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	" \
	: "=a" (ret) \
	: "r" (a), "r" (b), "r" (c), "r" (d), "r" (e), "r" (location))

#endif
