#ifndef __NET_IPV4_H
#define __NET_IPV4_H
#include <sea/types.h>
#include <sea/net/interface.h>
#include <sea/asm/system.h>
#include <sea/cpu/time.h>
#include <sea/ll.h>
struct ipv4_header {
#ifdef LITTLE_ENDIAN
	uint32_t header_len : 4;
	uint32_t version : 4;
#else
	uint32_t version : 4;
	uint32_t header_len : 4;
#endif
	uint8_t   tos;
	uint16_t  length;
	uint16_t  id;
	uint16_t  frag_offset;
	uint8_t   ttl;
	uint8_t   ptype;
	uint16_t  checksum;
	uint32_t  src_ip;
	uint32_t  dest_ip;
	unsigned char data[];
} __attribute__ ((packed));

struct ipv4_packet {
	time_t enqueue_time, last_attempt_time;
	int tries;
	struct net_packet *netpacket;
	struct ipv4_header *header;
} __attribute__ ((packed));

union ipv4_address {
	uint32_t address;
	uint8_t addr_bytes[4];
};

struct ipv4_fragment {
	struct net_packet *netpacket;
	struct ipv4_header *header;
	size_t total_length;
	int complete;
	uint32_t src, dest;
	uint8_t prot;
	uint16_t id;
	time_t start_time;
	struct llistnode *node;
	sint16_t first_hole;
};

void ipv4_receive_packet(struct net_dev *nd, struct net_packet *, struct ipv4_header *);
void ipv4_init();
int ipv4_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header);
int ipv4_copy_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header);

#define NETWORK_PREFIX(addr,mask) (addr & mask)
#define HOST_ADDRESS_PART(addr,mask) (addr & ~mask)
#define BROADCAST_ADDRESS(addr,mask) (addr | ~mask)

#define IP_PROTOCOL_ICMP 1

#define IP_FLAG_MF      (1 << 1)

#define FRAG_TIMEOUT 30

#endif

