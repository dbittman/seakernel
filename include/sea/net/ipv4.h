#ifndef __NET_IPV4_H
#define __NET_IPV4_H
#include <sea/types.h>
#include <sea/net/interface.h>

struct ipv4_packet {
	
};

union ipv4_address {
	uint32_t address;
	uint8_t addr_bytes[4];
};

void ipv4_receive_packet(struct net_dev *nd, struct ipv4_packet *);

#define NETWORK_PREFIX(addr,mask) (addr & mask)
#define HOST_ADDRESS_PART(addr,mask) (addr & ~mask)

#endif
