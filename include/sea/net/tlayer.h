#ifndef __SEA_NET_TLAYER_H
#define __SEA_NET_TLAYER_H

#include <sea/net/packet.h>
#include <sea/types.h>
#include <sea/fs/socket.h>

#define TLPROT_FLAG_CONNECTIONLESS 0x1

struct tlayer_prot_interface {
	int min_port, max_port;
	int start_ephemeral, end_ephemeral;
	int flags;
	int  (*verify)(struct net_packet *, void *payload, size_t len);
	void (*inject_port)(struct net_packet *, void *payload, size_t len, struct sockaddr *src, struct sockaddr *dest);
	int  (*recv_packet)(struct socket *, struct sockaddr *src, struct net_packet *, void *, size_t);
};


void net_tlayer_init();
int net_tlayer_recvfrom_network(struct sockaddr *src, struct sockaddr *dest, struct net_packet *np,
		int prot, void *payload, size_t len);
int net_tlayer_sendto_network(struct socket *socket, struct sockaddr *src, struct sockaddr *dest, void *payload, size_t len);
int net_tlayer_bind_socket(struct socket *sock, struct sockaddr *addr);
int net_tlayer_unbind_socket(struct socket *sock, struct sockaddr *addr);
int net_tlayer_register_protocol(int prot, struct tlayer_prot_interface *inter);
int net_tlayer_deregister_protocol(int prot);
#endif

