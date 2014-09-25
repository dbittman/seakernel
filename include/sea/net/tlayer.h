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
	struct {
		int  (*verify)(struct net_packet *, void *payload, size_t len);
		void (*inject_port)(struct net_packet *, void *payload, size_t len, struct sockaddr *src, struct sockaddr *dest);
		int  (*recv_packet)(struct socket *, struct sockaddr *src, struct net_packet *, void *, size_t);
		int  (*send_packet)(struct socket *, struct sockaddr *src, struct net_packet *, void *, size_t);
	} calls;
};

#endif

