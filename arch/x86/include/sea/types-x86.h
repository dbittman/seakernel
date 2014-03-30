#ifndef TYPES_x86_H
#define TYPES_x86_H

typedef unsigned int   u32int;
typedef          int   s32int;
typedef unsigned long long   u64int;
typedef          long long   s64int;
typedef unsigned short u16int;
typedef          short s16int;
typedef unsigned char  u8int;
typedef          char  s8int;

typedef unsigned int   u32;
typedef          int   s32;
typedef unsigned long long   u64;
typedef          long long   s64;
typedef unsigned short u16;
typedef          short s16;
typedef unsigned char  u8;
typedef          char  s8;

typedef unsigned int addr_t;

typedef unsigned int   uint32;
typedef          int   sint32;
typedef unsigned long long   uint64;
typedef          long long   sint64;
typedef unsigned short uint16;
typedef          short sint16;
typedef unsigned char  uint8;
typedef          char  sint8;

typedef unsigned int   uint32_t;
typedef          int   sint32_t;
typedef unsigned long long   uint64_t;
typedef          long long   sint64_t;
typedef unsigned short uint16_t;
typedef          short sint16_t;
typedef unsigned char  uint8_t;
typedef          char  sint8_t;
typedef unsigned int    intptr_t;
typedef s32 off_t;
typedef u32 size_t;
typedef s32 uid_t;
typedef s32 gid_t;
typedef s32 dev_t;
/* internally we handle this as a 32-bit integer to allow for it to not fuck up system call registers */
typedef u32 mode_t;
typedef signed long time_t;
typedef long pid_t;
#define NULL 0

#define FALSE 0
#define TRUE !FALSE
#  define _SYS_TYPES_FD_SET
#  define	NBBY	8		/* number of bits in a byte */
/*
 * Select uses bit masks of file descriptors in longs.
 * These macros manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here
 * should be >= NOFILE (param.h).
 */
#  ifndef	FD_SETSIZE
#	define	FD_SETSIZE	64
#  endif

typedef	long	fd_mask;
#  define	NFDBITS	(sizeof (fd_mask) * NBBY)	/* bits per mask */
#  ifndef	howmany
#	define	howmany(x,y)	(((x)+((y)-1))/(y))
#  endif

/* We use a macro for fd_set so that including Sockets.h afterwards
 *can work.  */
typedef	struct _types_fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} _types_fd_set;

#define fd_set _types_fd_set

#  define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1L << ((n) % NFDBITS)))
#  define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1L << ((n) % NFDBITS)))
#  define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1L << ((n) % NFDBITS)))
#  define	FD_ZERO(p)	(__extension__ (void)({ \
size_t __i; \
char *__tmp = (char *)p; \
for (__i = 0; __i < sizeof (*(p)); ++__i) \
	*__tmp++ = 0; \
	}))
#endif
