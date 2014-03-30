/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		NO Copyright (C) 1991, 1992 Linus Torvalds,
 *		consider these trivial functions to be PD.
 */

/*
 * Copyright (C) 2000-2005 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

/*
 * Modified for uClibc by Erik Andersen <andersen@codepoet.org>
 * These make no attempt to use nifty things like mmx/3dnow/etc.
 * These are not inline, and will therefore not be as fast as
 * modifying the headers to use inlines (and cannot therefore
 * do tricky things when dealing with const memory).  But they
 * should (I hope!) be faster than their generic equivalents....
 *
 * More importantly, these should provide a good example for
 * others to follow when adding arch specific optimizations.
 *  -Erik
 */

#include <sea/string.h>

size_t strnlen(const char *s, size_t count)
{
    int d0;
    register int __res;
    __asm__ __volatile__(
	    "movl %2,%0\n\t"
	    "incl %1\n"
	    "jmp 2f\n"
	    "1:\tcmpb $0,(%0)\n\t"
	    "je 3f\n\t"
	    "incl %0\n"
	    "2:\tdecl %1\n\t"
	    "jne 1b\n"
	    "3:\tsubl %2,%0"
	    :"=a" (__res), "=&d" (d0)
	    :"c" (s),"1" (count));
    return __res;
}
