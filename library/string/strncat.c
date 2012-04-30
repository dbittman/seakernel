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

#include <string.h>

/* Experimentally off - libc_hidden_proto(strncat) */
char *strncat(char * dest,
	const char * src, size_t count)
{
    int d0, d1, d2, d3;
    __asm__ __volatile__(
	    "repne\n\t"
	    "scasb\n\t"
	    "decl %1\n\t"
	    "movl %8,%3\n"
	    "incl %3\n"
	    "1:\tdecl %3\n\t"
	    "jz 2f\n"
	    "lodsb\n\t"
	    "stosb\n\t"
	    "testb %%al,%%al\n\t"
	    "jne 1b\n"
	    "jmp 3f\n"
	    "2:\txorl %2,%2\n\t"
	    "stosb\n"
	    "3:"
	    : "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
	    : "0" (src),"1" (dest),"2" (0),"3" (0xffffffff), "g" (count)
	    : "memory");
    return dest;
}
