#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/net/ethernet.h>

void net_data_send(struct net_dev *nd, struct net_packet *packet, int etype, uint8_t dest[6], int payload_len)
{
	ethernet_construct_header(packet->data_header, nd->hw_address, dest, etype);
	packet->length = payload_len + sizeof(struct ethernet_header);
	ethernet_send_packet(nd, packet);
}

void net_data_receive(struct net_dev *nd, struct net_packet *packet)
{
	ethernet_receive_packet(nd, packet);
}

