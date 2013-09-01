#ifndef ISR_x86_H
#define ISR_x86_H

typedef struct __attribute__((packed))
{
  volatile   u32int ds;
  volatile   u32int edi, esi, ebp, esp, ebx, edx, ecx, eax;
  volatile   u32int int_no, err_code;
  volatile   u32int eip, cs, eflags, useresp, ss;
} volatile registers_t;

#include <isr-x86_common.h>

#endif
