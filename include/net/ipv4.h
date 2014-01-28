#ifndef __NET_IPV4_H
#define __NET_IPV4_H

#include <net/net.h>

struct ipv4_packet {
	
};

void ipv4_receive_packet(struct net_dev *nd, struct ipv4_packet *);

#endif
