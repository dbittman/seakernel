#ifndef __SEA_NET_DATA_QUEUE
#define __SEA_NET_DATA_QUEUE

#include <sea/lib/queue.h>
#include <sea/net/packet.h>
struct ndq_item {
	struct net_packet *packet;
	void *data;
	size_t length;
};

#endif

