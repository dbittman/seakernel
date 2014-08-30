#ifndef NET_ARP_H
#define NET_ARP_H

#include <sea/net/packet.h>
#include <sea/types.h>
#include <sea/net/interface.h>

struct __attribute__((__packed__)) arp_packet {
	uint16_t hw_type;
	uint16_t p_type;
	uint8_t  hw_addr_len;
	uint8_t  p_addr_len;
	uint16_t oper;
	uint16_t src_hw_addr_1;
	uint16_t src_hw_addr_2;
	uint16_t src_hw_addr_3;
	uint16_t src_p_addr_1;
	uint16_t src_p_addr_2;
	uint16_t tar_hw_addr_1;
	uint16_t tar_hw_addr_2;
	uint16_t tar_hw_addr_3;
	uint16_t tar_p_addr_1;
	uint16_t tar_p_addr_2;
};

struct arp_entry {
	uint16_t prot_addr[2];
	uint16_t hw_addr[3];
	int hw_len, prot_len;
	int type;
	/* TODO: timestamps */
};

int arp_receive_packet(struct net_dev *nd, struct arp_packet *packet);

#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY   2

#endif
