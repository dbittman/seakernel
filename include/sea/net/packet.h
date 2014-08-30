#ifndef __NET_NET_H
#define __NET_NET_H

#include <sea/types.h>
#include <sea/net/interface.h>

struct net_packet {
	unsigned char data[0x1000];
	size_t length;
	size_t flags;
};

void net_notify_packet_ready(struct net_dev *nd);
int net_block_for_packets(struct net_dev *nd, struct net_packet *, int max);
void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count);

#endif
