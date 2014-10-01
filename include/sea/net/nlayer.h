#ifndef __SEA_NET_NLAYER_H
#define __SEA_NET_NLAYER_H

#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>

#define NLAYER_FLAG_HW_BROADCAST 1

struct nlayer_protocol {
	int flags;
	void (*receive)(struct net_dev *, struct net_packet *, void *payload);
	int  (*send)(void *, size_t len, struct sockaddr *, struct sockaddr *, int);
};

void net_nlayer_receive_from_dlayer(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, void *payload);
int net_nlayer_send_packet(void *payload, size_t len, struct sockaddr *dest, struct sockaddr *src, sa_family_t sa_family, int prot);

#endif

