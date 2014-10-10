#include <sea/net/interface.h>
#include <modules/udp.h>
#include <sea/net/packet.h>
#include <sea/net/tlayer.h>
#include <sea/net/data_queue.h>

#include <sea/fs/socket.h>

#include <sea/errno.h>
#include <sea/string.h>

static int recvfrom(struct socket *, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len);
static int sendto(struct socket *, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len);
static int shutdown(struct socket *sock, int how);
static int bind(struct socket *, const struct sockaddr *addr, socklen_t len);

static int  verify(struct net_packet *, void *payload, size_t len);
static void inject_port(struct net_packet *, void *payload, size_t len, struct sockaddr *src, struct sockaddr *dest);
static int  recv_packet(struct socket *, struct sockaddr *src, struct net_packet *, void *, size_t);

struct tlayer_prot_interface udp_tpi = {
	.max_port = 65535,
	.min_port = 0,
	.start_ephemeral = 49152,
	.end_ephemeral = 65535,
	.verify = verify,
	.inject_port = inject_port,
	.recv_packet = recv_packet,
};

struct socket_calls socket_calls_udp = {
	.init = 0,
	.accept = 0,
	.listen = 0,
	.connect = 0,
	.bind = bind,
	.shutdown = shutdown,
	.destroy = 0,
	.recvfrom = recvfrom,
	.sendto = sendto,
	.select = 0
};

static int verify(struct net_packet *np, void *payload, size_t len)
{
	/* TODO */
	return 0;
}

static void inject_port(struct net_packet *np, void *payload, size_t len, struct sockaddr *src, struct sockaddr *dest)
{
	struct udp_header *h = payload;
	*(uint16_t *)(src->sa_data) = h->src_port;
	*(uint16_t *)(dest->sa_data) = h->dest_port;
}

static int recv_packet(struct socket *sock, struct sockaddr *src, struct net_packet *np, void *data, size_t len)
{
	net_data_queue_enqueue(&sock->rec_data_queue, np, (uint8_t *)data + sizeof(struct udp_header), len, src, 0);
	return 0;
}

static int bind(struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
	if(sock->flags & SOCK_FLAG_BOUND)
		net_tlayer_unbind_socket(sock, (struct sockaddr *)addr);
	return net_tlayer_bind_socket(sock, (struct sockaddr *)addr);
}

static int recvfrom(struct socket *sock, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	return 0;
}

static int sendto(struct socket *sock, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len)
{
	if(!addr)
		return -EINVAL; /* we require sendto, not send */
	struct sockaddr zero;
	memset(&zero, 0, sizeof(zero));
	zero.sa_family = AF_INET;
	if(!(sock->flags & SOCK_FLAG_BOUND)) {
		/* grab ourselves a port to send from */
		int ret = sys_bind(sock->fd, &zero, addr_len);
		if(ret < 0)
			return ret;
	}
	unsigned char tmp[length + sizeof(struct udp_header)];
	memcpy(tmp + sizeof(struct udp_header), buffer, length);
	struct udp_header *uh = (void *)tmp;
	uh->src_port = *(uint16_t *)(sock->local.sa_data);
	uh->length = length + sizeof(struct udp_header);
	uh->checksum = 0; /* TODO */
	uh->dest_port = *(uint16_t *)(addr->sa_data);
	return net_tlayer_sendto_network(sock, &sock->local, addr, (void *)tmp, length + sizeof(struct udp_header));
}

static int shutdown(struct socket *sock, int how)
{
	if(socket_unbind(sock))
		net_tlayer_unbind_socket(sock, &sock->local);
	return 0;
}

int module_install()
{
	net_tlayer_register_protocol(PROTOCOL_UDP, &udp_tpi);
	socket_set_calls(17, &socket_calls_udp);
	return 0;
}

int module_exit()
{
	net_tlayer_deregister_protocol(PROTOCOL_UDP);
	socket_set_calls(17, 0);
	return 0;
}

