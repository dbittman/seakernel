#include <sea/fs/socket.h>
#include <sea/net/ipv4sock.h>
#include <sea/net/ipv4.h>
#include <sea/net/packet.h>
#include <sea/ll.h>
#include <sea/errno.h>
#include <sea/lib/queue.h>
#include <sea/net/data_queue.h>

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
	.sendto = sendto,
	.select = 0
};

static struct llist *sock_list =0;

static int recvfrom(struct socket *sock, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	if(!sock_list)
		return -EINVAL;
	return 0;
}

static int sendto(struct socket *sock, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len)
{
	if(!sock_list)
		return -EINVAL;
	uint8_t tmp[length + 20];
	struct ipv4_header *head;
	if(sock->sopt_levels[PROTOCOL_IP][IP_HDRINCL]) {
		head = (void *)buffer;
	} else {
		head = (struct ipv4_header *)tmp;
		memset(tmp, 0, 20);
		memcpy(tmp + 20, buffer, length);
		head->dest_ip = addr->sa_data[2] | (addr->sa_data[3] << 8)
			| (addr->sa_data[4] << 16) | (addr->sa_data[5] << 24);
		head->ttl = 64;
		head->length = HOST_TO_BIG16(length + 20);
		head->id = 0;
		head->ptype = sock->prot;
	}
	struct net_packet *packet = net_packet_create(0, 0);
	int ret;
	ret = ipv4_copy_enqueue_packet(packet, head);
	net_packet_put(packet, 0);
	return ret < 0 ? ret : (int)length;
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
	if(!socket_unbind(sock))
		return 0;
	ll_remove(sock_list, sock->node);
	sock->node = 0;
	return 0;
}

void ipv4_copy_to_sockets(struct net_packet *packet, struct ipv4_header *header)
{
	if(!sock_list)
		return;
	rwlock_acquire(&sock_list->rwl, RWL_READER);
	struct llistnode *node;
	struct socket *sock;
	struct sockaddr addr;
	memset(&addr, 0, sizeof(addr));
	addr.sa_data[2] = header->src_ip & 0xFF;
	addr.sa_data[3] = (header->src_ip >> 8) & 0xFF;
	addr.sa_data[4] = (header->src_ip >> 16) & 0xFF;
	addr.sa_data[5] = (header->src_ip >> 24) & 0xFF;
	addr.sa_family = AF_INET;
	ll_for_each_entry(sock_list, node, struct socket *, sock) {
		if(header->ptype == sock->prot || sock->prot == IPPROTO_RAW) {
			packet->flags |= NP_FLAG_NOWR;
			net_data_queue_enqueue(&sock->rec_data_queue, packet, header,
					BIG_TO_HOST16(header->length), &addr, 0);
		}
	}
	rwlock_release(&sock_list->rwl, RWL_READER);
}

