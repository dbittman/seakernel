#include <sea/net/packet.h>
#include <sea/net/ethernet.h>
#include <sea/net/interface.h>
#include <sea/net/arp.h>
#include <sea/net/nlayer.h>
#include <sea/fs/socket.h>
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

void ethernet_send_packet(struct net_dev *nd, struct net_packet *netpacket)
{
	if(netpacket->length < 60)
		netpacket->length = 60;
	TRACE(0, "[ethernet]: send packet size %d\n", netpacket->length);
	net_transmit_packet(nd, netpacket, 1);
}

sa_family_t ethernet_get_sa_family(int ethertype)
{
	switch(ethertype) {
		case ETHERTYPE_ARP:
			return AF_ARP;
		case ETHERTYPE_IPV4:
			return AF_INET;
	}
	return (sa_family_t)-1;
}

int ethernet_convert_sa_family(sa_family_t sa)
{
	switch(sa) {
		case AF_INET:
			return ETHERTYPE_IPV4;
		case AF_ARP:
			return ETHERTYPE_ARP;
	}
	return -1;
}

void ethernet_receive_packet(struct net_dev *nd, struct net_packet *packet)
{
	struct ethernet_header *head = (struct ethernet_header *)packet->data;
	unsigned char *payload = (unsigned char *)(head+1);
	TRACE(0, "[ethernet]: receive packet size %d\n", packet->length);
	sa_family_t af = ethernet_get_sa_family(BIG_TO_HOST16(head->type));
	if(af == (sa_family_t)-1)
		return;
	net_nlayer_receive_from_dlayer(nd, packet, af, payload);
}

