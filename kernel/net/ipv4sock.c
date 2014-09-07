#include <sea/fs/socket.h>
#include <sea/net/ipv4sock.h>
#include <sea/net/ipv4.h>
#include <sea/net/packet.h>
#include <sea/ll.h>
#include <sea/errno.h>
#include <sea/lib/queue.h>

static int recvfrom(struct socket *, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len);
static int sendto(struct socket *, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len);
static int init(struct socket *sock);
static int shutdown(struct socket *sock, int how);

struct socket_calls socket_calls_rawipv4 = {
	.init = init,
	.accept = 0,
	.listen = 0,
	.connect = 0,
	.bind = 0,
	.shutdown = shutdown,
	.destroy = 0,
	.recvfrom = recvfrom,
	.sendto = sendto
};

static struct llist *sock_list =0;

static int recvfrom(struct socket *sock, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	if(!sock_list)
		return -EINVAL;
	/* TODO: better blocking */
	
}

static int sendto(struct socket *sock, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len)
{
	if(!sock_list)
		return -EINVAL;
	/* TODO: check if we need to construct a header */
}

static int init(struct socket *sock)
{
	if(!sock_list)
		sock_list = ll_create(0);
	sock->node = ll_insert(sock_list, sock);
	return 0;
}

static int shutdown(struct socket *sock, int how)
{
	if(!sock_list)
		return 0;
	ll_remove(sock_list, sock->node);
	sock->node = 0;
	return 0;
}

void ipv4_copy_to_sockets(struct net_packet *packet, struct ipv4_header *header)
{
	if(!sock_list)
		return 0;
}

