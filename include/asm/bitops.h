#ifndef BITOPS_H
#define BITOPS_H

#define ADDR (*(volatile long *) addr)
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)

int __test_and_clear_bit(int nr, volatile void *addr);
int __test_and_set_bit(int nr, volatile void *addr);
unsigned long __ffs(unsigned long word);
unsigned long __find_next_zero_bit(const unsigned long *addr, unsigned long size,
				 unsigned long offset);

#define test_and_clear_bit(n, a) __test_and_clear_bit(n, a)
#define test_and_set_bit(n, a) __test_and_set_bit(n, a)
#define ffs(v) __ffs(v)
#define ffz(v) ffs(~v)
#define find_next_zero_bit(a, b, c) __find_next_zero_bit(a, b, c)

#endif
