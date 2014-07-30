#ifndef __ASM_SYSTEM_X86_64_H
#define __ASM_SYSTEM_X86_64_H

#define arch_cpu_jump(x) asm("jmp *%0"::"r"(x))
#define arch_cpu_pause() asm("pause")
#define arch_cpu_halt() asm("hlt")

#define LITTLE_ENDIAN

#endif
