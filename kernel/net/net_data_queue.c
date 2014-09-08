#include <sea/lib/queue.h>
#include <sea/net/data_queue.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>
#include <sea/mm/kmalloc.h>

void net_data_queue_enqueue(struct queue *q, struct net_packet *packet, void *data_start, size_t data_len)
{
	struct ndq_item *n = kmalloc(sizeof(struct ndq_item));
	n->packet = packet;
	n->data = data_start;
	n->length = data_len;
	queue_enqueue(q, n);
}

void net_data_copy_out(struct socket *sock, struct queue *queue, void *buffer, size_t len)
{
	/* behavior depends on socket type, SOCK_DGRAM/SOCK_RAW or SOCK_STREAM */
	struct ndq_item *n = queue_peek(queue);
	if(sock->type == SOCK_DGRAM)
		queue_dequeue(queue);
}

