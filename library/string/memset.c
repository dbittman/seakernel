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

/* Experimentally off - libc_hidden_proto(memset) */
void *memset(void *s, int c, size_t count)
{
	int ret;
	char *ptr = (char *)s;
	if(count == 1)
	{
		*ptr = (char)c;
		return ptr;
	}
	while(count)
	{
		if(!(count % 4))
		{
			register unsigned l = (c << 24) | (c << 16) | (c << 8) | c;
			asm("rep stosl" : "=c"(ret) : "D"(ptr), "c"(count/4), "a"(l));
			break;
		}
		else if(!count % 2)
		{
			register unsigned short s = (c << 8) | c;
			asm("rep stosw" : "=c"(ret) : "D"(ptr), "c"(count/2), "a"(s));
			break;
		} else
		{
			int len = count % 4;
			if(count == 3)
				len=1;
			asm("rep stosb" : "=c"(ret) : "D"(ptr), "c"(len), "a"(c));
			ptr+=len;
			count-=len;
		}
	}
	return s;
}

