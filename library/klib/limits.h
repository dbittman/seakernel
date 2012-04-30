/*
 * Copyright (c) 2009 Rich Edelman <redelman at gmail dot com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __LIBKERNEL_LIMITS_H__
#define __LIBKERNEL_LIMITS_H__

/*
 * These are all defined in the ISO C99 Standard, sections 7.10 and 
 * 5.2.4.2.1 Sizes of Integer Types <limits.h>
 */

/* Number of bits in a 'char' */
#define CHAR_BIT        8

/* Minimum value of a signed char */
#define SCHAR_MIN       (-128)
/* Maximum value of a signed char */
#define SCHAR_MAX       127

/* Maximum value of an unsigned char. Minimum is 0 */
#define UCHAR_MAX       255

/* Maximum and minimum values a 'char' can hold. This DOES
 * differ if a 'char' is signed or unsigned, so we check the 
 * compiler.
 */
#ifdef __CHAR_UNSIGNED__ 
#define CHAR_MIN        0
#define CHAR_MAX        UCHAR_MAX
#else
#define CHAR_MIN        SCHAR_MIN
#define CHAR_MAX        SCHAR_MAX
#endif /* __CHAR_UNSIGNED__ */

/* Minimum value of a signed short integer */
#define SHRT_MIN        -32768
/* Maximum value of a signed short integer */
#define SHRT_MAX        32767

/* Maximum value of an unsigned short integer. minimum is 0. */
#define USHRT_MAX       65535

/* Minimum value of a signed integer */
#define INT_MIN         (-INT_MAX - 1)
/* Maximum value of a signed integer */
#define INT_MAX         2147483647

/* Maximum value of an unsigned integer. minimum is 0. */
#define UINT_MAX        4294967295U

/* Minimum value of a signed long integer */
#define LONG_MIN        (-LONG_MAX - 1L)
/* Maximum value of a signed long integer */
#ifdef __LP64_
#define LONG_MAX        9223372036854775807L
#else
#define LONG_MAX        2147483647L
#endif

/* Maximum value of an unsigned long integer */
#ifdef __LP64__
#define ULONG_MAX       18446744073709551615UL
#else
#define ULONG_MAX       4294967295UL
#endif

/* Minimum value of a signed long long integer */
#define LLONG_MIN       (-LLONG_MAX - 1LL)
/* Maximum value of a signed long long integer */
#define LLONG_MAX       9223372036854775807LL
/* Maximum value of an unsigned long long integer */
#define ULLONG_MAX      18446744073709551615ULL

#endif /* __LIBKERNEL_LIMITS_H__ */
