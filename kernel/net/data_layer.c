#include <sea/asm/system.h>
#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/net/ethernet.h>
#include <sea/net/nlayer.h>

#include <sea/fs/socket.h>

void net_data_send(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, uint8_t dest[6], int payload_len)
{
	int etype = ethernet_convert_sa_family(sa_family);
	if(nd->hw_type == NET_HWTYPE_LOOP) {
		*(uint16_t *)(packet->data) = HOST_TO_BIG16(etype);
		net_transmit_packet(nd, packet, 1);
	} else {
		ethernet_construct_header(packet->data_header, nd->hw_address, dest, etype);
		packet->length = payload_len + sizeof(struct ethernet_header);
		ethernet_send_packet(nd, packet);
	}
}

static int __loop_get_af(int type)
{
	switch(type) {
		case ETHERTYPE_ARP:
			return PF_ARP;
		case ETHERTYPE_IPV4:
			return PF_INET;
	}
	return -1;
}

void net_data_receive(struct net_dev *nd, struct net_packet *packet)
{
	if(nd->hw_type == NET_HWTYPE_LOOP) {
		packet->data_header = packet->data;
		unsigned char *payload = packet->data + sizeof(uint16_t);
		int af = __loop_get_af(BIG_TO_HOST16(*(uint16_t *)(packet->data)));
		net_nlayer_receive_from_dlayer(nd, packet, af, payload);
	} else {
		ethernet_receive_packet(nd, packet);
	}
}

