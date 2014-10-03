#ifndef __NET_ICMP_H
#define __NET_ICMP_H

#include <sea/types.h>

struct icmp_packet {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint32_t rest;
} __attribute__ ((packed));

void icmp_receive_packet(struct net_dev *nd, struct net_packet *netpacket, union ipv4_address src, struct icmp_packet *packet, int len);

#endif
