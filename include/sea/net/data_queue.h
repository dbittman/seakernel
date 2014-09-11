#ifndef __SEA_NET_DATA_QUEUE
#define __SEA_NET_DATA_QUEUE

#include <sea/lib/queue.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>

struct ndq_item {
	struct net_packet *packet;
	void *data;
	size_t length;
	struct sockaddr addr;
};

size_t net_data_queue_copy_out(struct socket *sock, struct queue *queue, void *buffer, size_t len, int peek, struct sockaddr *addr);
void net_data_queue_enqueue(struct queue *q, struct net_packet *packet, void *data_start, size_t data_len, struct sockaddr *);

#endif

