#ifndef BITOPS_H
#define BITOPS_H

#define ADDR (*(volatile long *) addr)
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)
#endif
