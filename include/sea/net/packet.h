#ifndef __NET_NET_H
#define __NET_NET_H

#include <sea/types.h>
#include <sea/net/interface.h>

#define MAX_PACKET_SIZE 0x1000

struct net_packet {
	unsigned char data[MAX_PACKET_SIZE];
	size_t length;
	int flags;

	void *data_header;
	void *network_header;

	volatile int count;
};

#define NP_FLAG_ALLOC 1
#define NP_FLAG_NOWR  2 /* don't allow changes to packet data */
#define NP_FLAG_FORW  4 /* packet is being forwarded */
#define NP_FLAG_NOFILLSRC 8

#define NP_FLAG_DESTROY 1 /* this is a flag to net_packet_put, and not a packet state flag.
						   * it tells put that it must destroy the packet, or panic. It would
						   * only panic if the count is >1 when calling put, so this is useful
						   * to ensure that a packet stored on the stack doesn't have a reference
						   * that goes out of scope */

void net_notify_packet_ready(struct net_dev *nd);
void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count);

struct net_packet *net_packet_create(struct net_packet *packet, int flags);
void net_packet_destroy(struct net_packet *packet);
void net_packet_get(struct net_packet *packet);
void net_packet_put(struct net_packet *packet, int);

#endif
