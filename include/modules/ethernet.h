#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/ethertype.h>
#include <sea/fs/socket.h>
struct ethernet_header {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t type;
};

void ethernet_receive_packet(struct net_dev *nd, struct net_packet *packet);
void ethernet_construct_header(struct ethernet_header *head, uint8_t src_mac[6], uint8_t dest_mac[6], uint16_t ethertype);
int ethernet_convert_sa_family(sa_family_t sa);
void ethernet_send_packet(struct net_dev *nd, struct net_packet *);
void ethernet_transmit_packet(struct net_dev *nd, struct net_packet *netpacket, sa_family_t sa, uint8_t dest[6], int len);


#endif
