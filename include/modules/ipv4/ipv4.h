#ifndef __NET_IPV4_H
#define __NET_IPV4_H
#include <stdint.h>
#include <sea/types.h>
#include <sea/net/interface.h>
#include <sea/asm/system.h>
#include <sea/cpu/time.h>
#include <sea/ll.h>
#include <sea/tm/kthread.h>
#include <sea/lib/queue.h>
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
	struct linkedentry node;
	int16_t first_hole;
};

void ipv4_receive_packet(struct net_dev *nd, struct net_packet *, void *);
int ipv4_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header);
int ipv4_copy_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header);
int ipv4_enqueue_sockaddr(void *payload, size_t len, struct sockaddr *addr, struct sockaddr *src, int prot);
int ipv4_sending_thread(struct kthread *kt, void *arg);
int __ipv4_cleanup_fragments(int do_remove);
uint16_t ipv4_calc_checksum(void *__data, int length);

#define NETWORK_PREFIX(addr,mask) (addr & mask)
#define HOST_ADDRESS_PART(addr,mask) (addr & ~mask)
#define BROADCAST_ADDRESS(addr,mask) (addr | ~mask)

#define IP_PROTOCOL_ICMP 1

#define IP_FLAG_MF      (1 << 1)
#define IP_FLAG_DF      (1 << 2)

#define FRAG_TIMEOUT 30

extern struct queue *ipv4_tx_queue;
extern struct kthread *ipv4_send_thread;
extern time_t ipv4_thread_lastwork;
extern struct linkedlist *frag_list;

#endif

