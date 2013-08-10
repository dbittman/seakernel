#ifndef BITOPS_X86_64_H
#define BITOPS_X86_64_H

#define ADDR (*(volatile long *) addr)
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)
#endif
