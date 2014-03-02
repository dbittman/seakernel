#ifndef __ARCH_SEA_CPU_CMOS_X86_COMMON_H
#define __ARCH_SEA_CPU_CMOS_X86_COMMON_H

void cmos_write(unsigned char addr, unsigned int value);
unsigned char cmos_read(unsigned char addr);

#endif
