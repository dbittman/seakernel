#include <sea/lib/queue.h>
#include <sea/net/data_queue.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>

void net_data_queue_enqueue(struct queue *q, struct net_packet *packet, void *data_start, size_t data_len, struct sockaddr *addr)
{
	struct ndq_item *n = kmalloc(sizeof(struct ndq_item));
	n->packet = packet;
	net_packet_get(packet);
	n->data = data_start;
	n->length = data_len;
	memcpy(&n->addr, addr, sizeof(*addr));
	queue_enqueue(q, n);
}

size_t net_data_queue_copy_out(struct socket *sock, struct queue *queue, void *buffer, size_t len, int peek, struct sockaddr *addr)
{
	size_t rem=len, nbytes=0;
	/* behavior depends on socket type, SOCK_DGRAM/SOCK_RAW or SOCK_STREAM */
	int packet_based = (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW);
	if(addr)
		memset(addr, 0, sizeof(*addr));
	while(rem > 0) {
		struct ndq_item *n = queue_peek(queue);
		if(!n)
			return nbytes;
		
		if(memcmp(addr, &n->addr, sizeof(*addr)) && nbytes) {
			/* different source! */
			return nbytes;
		}

		size_t copy_length = rem > n->length ? n->length : rem;
		memcpy((uint8_t *)buffer + nbytes, n->data, copy_length);
		nbytes += copy_length;
		rem -= copy_length;
		n->length -= copy_length;
		n->data = (void *)((addr_t)n->data + copy_length);

		if(addr)
			memcpy(addr, &n->addr, sizeof(*addr));

		if((packet_based || n->length == 0) && !peek) {
			queue_dequeue(queue);
			net_packet_put(n->packet, 0);
			kfree(n);
		}
		if(packet_based || peek /* TODO: peek more data */)
			break;
	}
	return nbytes;
}

