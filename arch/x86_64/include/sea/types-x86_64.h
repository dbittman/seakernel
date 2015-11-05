#ifndef TYPES_x86_64_H
#define TYPES_x86_64_H

#include <stdint.h>

typedef uint64_t addr_t;

typedef int64_t off_t;
typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef int32_t uid_t;
typedef int32_t gid_t;
typedef int32_t dev_t;
/* internally we handle this as a 32-bit integer to allow for it to not fuck up system call registers */
typedef uint32_t mode_t;
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
	
