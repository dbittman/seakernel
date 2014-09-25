#ifndef __SEA_FS_SOCKET_H
#define __SEA_FS_SOCKET_H

#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/fs/file.h>
#include <sea/ll.h>
#include <sea/lib/queue.h>

typedef unsigned short sa_family_t;
typedef unsigned int socklen_t;

struct sockaddr {
	sa_family_t sa_family;
	unsigned char sa_data[14];
};

#define PROTOCOL_IP      0
#define PROTOCOL_ICMP    1
#define PROTOCOL_TCP     6
#define PROTOCOL_UDP     17
#define PROT_MAXPROT     17
#define IPPROTO_RAW 255

#define SOCK_STREAM  1       /* stream socket */
#define SOCK_DGRAM   2       /* datagram socket */
#define SOCK_RAW     3       /* raw-protocol interface */
#define SOCK_MAXTYPE 3

static int __socket_default_protocols_per_type[4] = {
	0,
	PROTOCOL_TCP, PROTOCOL_UDP,
	0
};

#define SOCK_FLAG_CONNECTING   0x1
#define SOCK_FLAG_CONNECTED    0x2
#define SOCK_FLAG_ALLOWSEND    0x4
#define SOCK_FLAG_ALLOWRECV    0x8

#define socket_unbind(socket) (!((socket->flags & SOCK_FLAG_ALLOWSEND) || (socket->flags & SOCK_FLAG_ALLOWRECV)))

/*
 * Option flags per-socket.
 */
#define SO_DEBUG    0x0001      /* turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002      /* socket has had listen() */
#define SO_REUSEADDR    0x0004      /* allow local address reuse */
#define SO_KEEPALIVE    0x0008      /* keep connections alive */
#define SO_DONTROUTE    0x0010      /* just use interface addresses */
#define SO_BROADCAST    0x0020      /* permit sending of broadcast msgs */
#define SO_USELOOPBACK  0x0040      /* bypass hardware when possible */
#define SO_LINGER   0x0080      /* linger on close if data present */
#define SO_OOBINLINE    0x0100      /* leave received OOB data in line */
#define SO_REUSEPORT    0x0200      /* allow local address & port reuse */
#define SO_TIMESTAMP    0x0400      /* timestamp received dgram traffic */
#define SO_ACCEPTFILTER 0x1000      /* there is an accept filter */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF   0x1001      /* send buffer size */
#define SO_RCVBUF   0x1002      /* receive buffer size */
#define SO_SNDLOWAT 0x1003      /* send low-water mark */
#define SO_RCVLOWAT 0x1004      /* receive low-water mark */
#define SO_SNDTIMEO 0x1005      /* send timeout */
#define SO_RCVTIMEO 0x1006      /* receive timeout */
#define SO_ERROR    0x1007      /* get error status and clear */
#define SO_TYPE     0x1008      /* get socket type */
/*
 Level number for (get/set)sockopt() to apply to socket itself.
*/
#define SOL_SOCKET  0xffff      /* options for socket level */

/* Protocol families.  */
#define PF_UNSPEC   0   /* Unspecified.  */
#define PF_LOCAL    1   /* Local to host (pipes and file-domain).  */
#define PF_INET     2
#define PF_UNIX     PF_LOCAL /* Old BSD name for PF_LOCAL.  */
#define PF_FILE     PF_LOCAL /* Another non-standard name for PF_LOCAL.  */
#define PF_MAX      32  /* For now..  */

/* Address families.  */
#define AF_LOCAL    PF_LOCAL
#define AF_UNIX     PF_UNIX
#define AF_FILE     PF_FILE
#define AF_INET     PF_INET

extern uint16_t af_to_ethertype_map[PF_MAX];
struct sockproto {
	uint16_t sp_family;      /* address family */
	uint16_t sp_protocol;        /* protocol */
};

#define SOMAXCONN   128

#define MSG_OOB     0x1     /* process out-of-band data */
#define MSG_PEEK    0x2     /* peek at incoming message */
#define MSG_DONTROUTE   0x4     /* send without using routing tables */
#define MSG_EOR     0x8     /* data completes record */
#define MSG_TRUNC   0x10        /* data discarded before delivery */
#define MSG_CTRUNC  0x20        /* control data lost before delivery */
#define MSG_WAITALL 0x40        /* wait for full request or error */
#define MSG_DONTWAIT    0x80        /* this message should be nonblocking */
#define MSG_EOF     0x100       /* data completes connection */
#define MSG_COMPAT      0x8000      /* used in sendit() */
#define SHUT_RD     0       /* shut down the reading side */
#define SHUT_WR     1       /* shut down the writing side */
#define SHUT_RDWR   2       /* shut down both sides */

#define IP_HDRINCL              2

struct socket;

struct socket_calls {
	int (*init)(struct socket *);
	int (*connect)(struct socket *, const struct sockaddr *addr, socklen_t len);
	struct socket * (*accept)(struct socket *, struct sockaddr *restrict addr, socklen_t *restrict len, int *err);
	int (*listen)(struct socket *, int backlog);
	int (*bind)(struct socket *, const struct sockaddr *addr, socklen_t len);
	int (*shutdown)(struct socket *, int how);
	int (*recvfrom)(struct socket *, void *buffer, size_t length,
			int flags, struct sockaddr *addr, socklen_t *addr_len);
	int (*sendto)(struct socket *, const void *buffer, size_t length,
			int flags, struct sockaddr *addr, socklen_t addr_len);
	int (*destroy)(struct socket *);
	int (*select)(struct socket *, int);
};

struct socket {
	int flags;
	int domain;
	int type;
	int prot;	
	int sopt;
	int fd;
	int sopt_extra[16];
	int sopt_extra_sizes[16];
	int sopt_levels[18][64];
	int sopt_levels_sizes[18][64];

	struct socket_calls *calls;
	struct sockaddr peer, local;
	socklen_t peer_len, local_len;

	struct inode *inode;
	struct file *file;

	struct llistnode *node;
	struct queue rec_data_queue;
};

struct socket_fromto_info {
    int sock;
    void *buffer;
    size_t len;
    int flags;
    struct sockaddr *addr;
    socklen_t *addr_len;
};

struct socket *socket_create(int *errcode);
void socket_set_calls(int prot, struct socket_calls *calls);
int sys_socket(int domain, int type, int prot);
int sys_connect(int socket, const struct sockaddr *addr, socklen_t len);
int sys_accept(int socket, struct sockaddr *restrict addr, socklen_t *restrict addr_len);
int sys_listen(int socket, int backlog);
int sys_bind(int socket, const struct sockaddr *address, socklen_t address_len);
int sys_getsockopt(int socket, int level, int option_name,
		void *restrict option_value, socklen_t *restrict option_len);
int sys_setsockopt(int socket, int level, int option_name,
		const void *option_value, socklen_t option_len);
int sys_sockshutdown(int socket, int how);
ssize_t sys_recv(int socket, void *buffer, size_t length, int flags);
ssize_t sys_send(int socket, const void *buffer, size_t length, int flags);
int sys_getsockname(int socket, struct sockaddr *restrict address,
		socklen_t *restrict address_len);
int sys_recvfrom(struct socket_fromto_info *m);
int sys_sendto(struct socket_fromto_info *m);
int socket_select(struct file *file, int rw);
#endif

