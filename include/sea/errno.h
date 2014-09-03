#ifndef _SEA_ERRNO_H
#define _SEA_ERRNO_H


#define EPERM 1         /* Not super-user */
#define ENOENT 2        /* No such file or directory */
#define ESRCH 3         /* No such process */
#define EINTR 4         /* Interrupted system call */
#define EIO 5           /* I/O error */
#define ENXIO 6         /* No such device or address */
#define E2BIG 7         /* Arg list too long */
#define ENOEXEC 8       /* Exec format error */
#define EBADF 9         /* Bad file number */
#define ECHILD 10       /* No children */
#define EAGAIN 11       /* No more processes */
#define ENOMEM 12       /* Not enough core */
#define EACCES 13       /* Permission denied */
#define EFAULT 14       /* Bad address */

#define EBUSY 16        /* Mount device busy */
#define EEXIST 17       /* File exists */
#define EXDEV 18        /* Cross-device link */
#define ENODEV 19       /* No such device */
#define ENOTDIR 20      /* Not a directory */
#define EISDIR 21       /* Is a directory */
#define EINVAL 22       /* Invalid argument */
#define ENFILE 23       /* Too many open files in system */
#define EMFILE 24       /* Too many open files */
#define ENOTTY 25       /* Not a typewriter */
#define ETXTBSY 26      /* Text file busy */
#define EFBIG 27        /* File too large */
#define ENOSPC 28       /* No space left on device */
#define ESPIPE 29       /* Illegal seek */
#define EROFS 30        /* Read only file system */
#define EMLINK 31       /* Too many links */
#define EPIPE 32        /* Broken pipe */
#define EDOM 33         /* Math arg out of domain of func */
#define ERANGE 34       /* Math result not representable */
#define ENOMSG 35       /* No message of desired type */
#define EIDRM 36        /* Identifier removed */
#define EDEADLK 45      /* Deadlock condition */
#define ENOLCK 46       /* No record locks available */
#define ENOSTR 60       /* Device not a stream */
#define ENODATA 61      /* No data (for no delay io) */
#define ETIME 62        /* Timer expired */
#define ENOSR 63        /* Out of streams resources */
#define ENOLINK 67      /* The link has been severed */
#define EPROTO 71       /* Protocol error */
#define EMULTIHOP 74    /* Multihop attempted */
#define EBADMSG 77      /* Trying to read unreadable message */
#define EFTYPE 79       /* Inappropriate file type or format */
#define ENOSYS 88       /* Function not implemented */
#define ENOTEMPTY 90    /* Directory not empty */
#define ENAMETOOLONG 91 /* File or path name too long */
#define ELOOP 92        /* Too many symbolic links */
#define EOPNOTSUPP 95   /* Operation not supported on transport endpoint */
#define EPFNOSUPPORT 96 /* Protocol family not supported */
#define ECONNRESET 104  /* Connection reset by peer */
#define ENOBUFS 105     /* No buffer space available */
#define EAFNOSUPPORT 106 /* Address family not supported by protocol family */
#define EPROTOTYPE 107  /* Protocol wrong type for socket */
#define ENOTSOCK 108    /* Socket operation on non-socket */
#define ENOPROTOOPT 109 /* Protocol not available */
#define ESHUTDOWN 110   /* Can't send after socket shutdown */
#define ECONNREFUSED 111        /* Connection refused */
#define EADDRINUSE 112          /* Address already in use */
#define ECONNABORTED 113        /* Connection aborted */
#define ENETUNREACH 114         /* Network is unreachable */
#define ENETDOWN 115            /* Network interface is not configured */
#define ETIMEDOUT 116           /* Connection timed out */
#define EHOSTDOWN 117           /* Host is down */
#define EHOSTUNREACH 118        /* Host is unreachable */
#define EINPROGRESS 119         /* Connection already in progress */
#define EALREADY 120            /* Socket already connected */
#define EDESTADDRREQ 121        /* Destination address required */
#define EMSGSIZE 122            /* Message too long */
#define EPROTONOSUPPORT 123     /* Unknown protocol */
#define EADDRNOTAVAIL 125       /* Address not available */
#define ENETRESET 126
#define EISCONN 127             /* Socket is already connected */
#define ENOTCONN 128            /* Socket is not connected */
#define ETOOMANYREFS 129
#define EDQUOT 132
#define ESTALE 133
#define ENOTSUP 134             /* Not supported */
#define EILSEQ 138
#define EOVERFLOW 139   /* Value too large for defined data type */
#define ECANCELED 140   /* Operation canceled */
#define ENOTRECOVERABLE 141     /* State not recoverable */
#define EOWNERDEAD 142  /* Previous owner died */
#define EWOULDBLOCK EAGAIN      /* Operation would block */

/* Should never be seen by user programs */
#define ERESTARTSYS	512
#define ERESTARTNOINTR	513
#define ERESTARTNOHAND	514	/* restart if no handler.. */

#endif

