/* provides interface between the network layer and specific protocols, 
 * and keeps a port pool for each protocol.
 */

#include <sea/asm/system.h>
#include <sea/string.h>
#include <sea/kernel.h>
#include <sea/net/packet.h>
#include <sea/net/tlayer.h>
#include <sea/fs/socket.h>

#include <sea/lib/hash.h>
#include <sea/errno.h>

#include <sea/net/nlayer.h>

#define GET_PORT(addr) BIG_TO_HOST16(*(uint16_t *)(addr)->sa_data)
#define SET_PORT(addr,p) (*(uint16_t *)((addr)->sa_data) = HOST_TO_BIG16(p))

static struct tlayer_prot_interface *protocols[PROT_MAXPROT];
static struct hash_table pool[PROT_MAXPROT];

static struct tlayer_prot_interface *net_tlayer_get_tpi(int prot)
{
	return protocols[prot];
}

int net_tlayer_register_protocol(int prot, struct tlayer_prot_interface *inter)
{
	if(protocols[prot])
		return -EBUSY;
	protocols[prot] = inter;
	hash_table_create(&pool[prot], 0, HASH_TYPE_CHAIN);
	hash_table_resize(&pool[prot], HASH_RESIZE_MODE_IGNORE, 100);
	hash_table_specify_function(&pool[prot], HASH_FUNCTION_DEFAULT);
	return 0;
}

int net_tlayer_deregister_protocol(int prot)
{
	if(!protocols[prot])
		return -ENOENT;
	protocols[prot] = 0;
	hash_table_destroy(&pool[prot]);
	return 0;
}

static struct socket *net_tlayer_get_socket(int prot, struct sockaddr *addr)
{
	if(!protocols[prot])
		return 0;
	void *value;
	/* so, we can just use the sockaddr as the key to a hash table */
	if(hash_table_get_entry(&pool[prot], addr, sizeof(*addr), 1, &value) == -ENOENT)
	{
		/* and also try the "any" address */
		struct sockaddr any;
		memcpy(&any, addr, sizeof(any));
		memset(any.sa_data, 0, sizeof(any.sa_data));
		SET_PORT(&any, GET_PORT(addr));
		if(hash_table_get_entry(&pool[prot], &any, sizeof(any), 1, &value) == -ENOENT)
			return 0;
	}
	return value;
}

static int net_tlayer_dynamic_port(int prot, struct sockaddr *addr)
{
	/* okay, this is slow, since we have to scan... */
	int p = protocols[prot]->start_ephemeral;
	for(;p<=protocols[prot]->end_ephemeral;p++) {
		SET_PORT(addr, p);
		if(!net_tlayer_get_socket(prot, addr))
			return p;
	}
	return -1;
}

int net_tlayer_bind_socket(struct socket *sock, struct sockaddr *addr)
{
	int prot = sock->prot;
	if(!protocols[prot])
		return -EINVAL;
	int port_num = GET_PORT(addr);
	if(!port_num) {
		port_num = net_tlayer_dynamic_port(prot, addr);
		SET_PORT(addr, port_num);
	}
	if(port_num == -1)
		return -EADDRINUSE;
	if(hash_table_get_entry(&pool[prot], addr, sizeof(*addr), 1, 0) != -ENOENT)
		return -EADDRINUSE;
	/* and also try the "any" address */
	struct sockaddr any;
	memcpy(&any, addr, sizeof(any));
	memset(any.sa_data, 0, sizeof(any.sa_data));
	SET_PORT(&any, GET_PORT(addr));
	if(hash_table_get_entry(&pool[prot], &any, sizeof(any), 1, 0) != -ENOENT)
		return -EADDRINUSE;
	return hash_table_set_entry(&pool[prot], addr, sizeof(*addr), 1, sock);
}

int net_tlayer_unbind_socket(struct socket *sock, struct sockaddr *addr)
{
	int prot = sock->prot;
	if(!protocols[prot])
		return -EINVAL;
	void *value;
	if(hash_table_get_entry(&pool[prot], addr, sizeof(*addr), 1, &value) == -ENOENT)
		return -ENOENT;
	assert(value == sock);
	hash_table_delete_entry(&pool[prot], addr, sizeof(*addr), 1);
	return 0;
}

/* len refers to payload length */
int net_tlayer_recvfrom_network(struct sockaddr *src, struct sockaddr *dest, struct net_packet *np,
		int prot, void *payload, size_t len)
{
	/* src refers to where the packet is from, and dest refers to which endpoint got the packet.
	 * they only contain network-level address, and not ports. We have a callback into the protocol
	 * that writes the ports into src and dest.
	 */
	struct tlayer_prot_interface *tpi;
	if(!(tpi = net_tlayer_get_tpi(prot)))
		return -ENOTSUP;

	/* verify the packet */
	if(tpi->verify) {
		if(tpi->verify(np, payload, len) < 0)
			return -EINVAL;
	}
	/* ask the protocol to fill in address data */
	if(tpi->inject_port)
		tpi->inject_port(np, payload, len, src, dest);
	/* get the socket from the port pool */
	struct socket *sock;
	if(!(sock = net_tlayer_get_socket(prot, dest)))
		return -ENOTCONN;
	if(tpi->recv_packet)
		tpi->recv_packet(sock, src, np, payload, len);
	return 0;
}

int net_tlayer_sendto_network(struct socket *socket, struct sockaddr *src, struct sockaddr *dest, void *payload, size_t len)
{
	return net_nlayer_send_packet(payload, len, dest, src, socket->domain, socket->prot);
}

void net_tlayer_init(void)
{
	memset(protocols, 0, sizeof(protocols));
}

