#ifndef __ARCH_SEA_CPU_REGISTERS_X86_64_H
#define __ARCH_SEA_CPU_REGISTERS_X86_64_H

#include <types.h>

typedef volatile struct __attribute__((packed)) 
{
	volatile   uint64_t ds;
	volatile   uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi;
	volatile   uint64_t int_no, err_code;
	volatile   uint64_t eip, cs, eflags, useresp, ss;
} registers_t;

#endif
