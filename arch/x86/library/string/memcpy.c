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

/* Experimentally off - libc_hidden_proto(memcpy) */
void *memcpy(void * to, const void * from, size_t n)
{
    int d0, d1, d2;
    __asm__ __volatile__(
	    "rep ; movsl\n\t"
	    "testb $2,%b4\n\t"
	    "je 1f\n\t"
	    "movsw\n"
	    "1:\ttestb $1,%b4\n\t"
	    "je 2f\n\t"
	    "movsb\n"
	    "2:"
	    : "=&c" (d0), "=&D" (d1), "=&S" (d2)
	    :"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
	    : "memory");
    return (to);
}

