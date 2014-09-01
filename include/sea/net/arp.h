#ifndef NET_ARP_H
#define NET_ARP_H

#include <sea/net/packet.h>
#include <sea/types.h>
#include <sea/net/interface.h>
#include <sea/ll.h>

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
	time_t timestamp;
	struct llistnode *node; /* for outstanding requests list */
};

int arp_receive_packet(struct net_dev *nd, struct arp_packet *packet);
void arp_send_request(struct net_dev *nd, uint16_t prot_type, uint8_t prot_addr[4], int addr_len);
int arp_lookup(int ptype, uint8_t paddr[4], uint8_t hwaddr[6]);
void arp_init();
#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY   2

#endif
