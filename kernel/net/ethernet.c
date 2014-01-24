#include <kernel.h>
#include <net/net.h>
#include <net/ethernet.h>

void ethernet_send_packet(struct net_dev *nd, struct ethernet_header *head, unsigned char *payload, int length)
{
	
}

void ethernet_receive_packet(struct net_dev *nd, struct net_packet *packet)
{
	struct ethernet_header *head = packet->data;
	unsigned char *payload = (unsigned char *)(head+1);
	unsigned length = packet->length;
}
