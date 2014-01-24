#include <kernel.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <asm/system.h>

void ethernet_construct_header(struct ethernet_header *head, uint8_t src_mac[6], uint8_t dest_mac[6], uint16_t ethertype)
{
	for(int i=0;i<6;i++)
	{
		head->dest_mac[i] = dest_mac[i];
		head->src_mac[i] = src_mac[i];
	}
	head->type = HOST_TO_BIG16(ethertype);
}

void ethernet_send_packet(struct net_dev *nd, struct ethernet_header *head, unsigned char *payload, int length)
{
	struct net_packet packet;
	memcpy(packet.data, head, sizeof(*head));
	memcpy((void *)((addr_t)packet.data + sizeof(*head)), payload, length);
	/* may need to insert checksum here...? */
	packet.length = length + sizeof(*head);
	net_transmit_packet(nd, &packet, 1);
}

void ethernet_receive_packet(struct net_dev *nd, struct net_packet *packet)
{
	struct ethernet_header *head = (struct ethernet_header *)packet->data;
	unsigned char *payload = (unsigned char *)(head+1);
	unsigned length = packet->length;
	switch(BIG_TO_HOST16(head->type)) {
		case ETHERTYPE_ARP:
			arp_receive_packet(nd, (struct arp_packet *)payload);
			break;
		default:
			kprintf("unknown ethertype\n");
			break;
	}
}
