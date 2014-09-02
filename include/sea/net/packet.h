#ifndef __NET_NET_H
#define __NET_NET_H

#include <sea/types.h>
#include <sea/net/interface.h>

struct net_packet {
	unsigned char data[0x1000];
	size_t length;
	size_t flags;

	void *data_header;
	void *network_header;
};

void net_notify_packet_ready(struct net_dev *nd);
void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count);

#endif
