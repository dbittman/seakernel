#ifndef ISR_x86_64_H
#define ISR_x86_64_H

#include <types.h>

typedef struct __attribute__((packed)) 
{
	volatile   u64int ds;
	volatile   u64int r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi;
	volatile   u64int int_no, err_code;
	volatile   u64int eip, cs, eflags, useresp, ss;
} volatile registers_t;

#include <isr-x86_common.h>

#endif
