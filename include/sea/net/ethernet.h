#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <sea/net/net.h>

struct ethernet_header {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t type;
};

void ethernet_receive_packet(struct net_dev *nd, struct net_packet *packet);
void ethernet_construct_header(struct ethernet_header *head, uint8_t src_mac[6], uint8_t dest_mac[6], uint16_t ethertype);
void ethernet_send_packet(struct net_dev *nd, struct ethernet_header *head, unsigned char *payload, int length);

#define ETHERTYPE_IPV4 0x800
#define ETHERTYPE_ARP  0x806

#endif
