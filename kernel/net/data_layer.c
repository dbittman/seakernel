#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/net/ethernet.h>
#include <sea/net/arp.h>
#include <sea/net/ipv4.h>

void net_data_send(struct net_dev *nd, struct net_packet *packet, int etype, uint8_t dest[6], int payload_len)
{
	if(nd->hw_type == NET_HWTYPE_LOOP) {
		*(uint16_t *)(packet->data) = HOST_TO_BIG16(etype);
		net_transmit_packet(nd, packet, 1);
	} else {
		ethernet_construct_header(packet->data_header, nd->hw_address, dest, etype);
		packet->length = payload_len + sizeof(struct ethernet_header);
		ethernet_send_packet(nd, packet);
	}
}

void net_data_receive(struct net_dev *nd, struct net_packet *packet)
{
	if(nd->hw_type == NET_HWTYPE_LOOP) {
		packet->data_header = packet->data;
		unsigned char *payload = packet->data + sizeof(uint16_t);
		switch(BIG_TO_HOST16(*(uint16_t *)(packet->data))) {
		case ETHERTYPE_ARP:
			arp_receive_packet(nd, packet, (struct arp_packet *)payload);
			break;
		case ETHERTYPE_IPV4:
			ipv4_receive_packet(nd, packet, (struct ipv4_header *)payload);
			break;
		default:
			kprintf("unknown ethertype\n");
			break;
		}
	} else {
		ethernet_receive_packet(nd, packet);
	}
}

