#include <sea/net/packet.h>
#include <sea/net/ethernet.h>
#include <sea/net/interface.h>
#include <sea/net/arp.h>
#include <sea/net/ipv4.h>
#include <sea/asm/system.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

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
	memset(packet.data, 0, 64);
	memcpy(packet.data, head, sizeof(*head));
	memcpy((void *)((addr_t)packet.data + sizeof(*head)), payload, length);
	packet.length = length + sizeof(*head);
	if(packet.length < 60) packet.length = 60;
	kprintf("ETH: Send packet size %d\n", packet.length);
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
		case ETHERTYPE_IPV4:
			ipv4_receive_packet(nd, (struct ipv4_header *)payload);
			break;
		default:
			kprintf("unknown ethertype\n");
			break;
	}
}
