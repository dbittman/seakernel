#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <modules/ipv4/ipv4.h>
#include <modules/ipv4/icmp.h>
#include <sea/vsprintf.h>
#include <sea/net/ethertype.h>

static uint16_t icmp_calc_checksum(void *__data, int length)
{
	uint8_t *data = __data;
	uint32_t sum = 0;
	int i;
	for(i=0;i+1<length;i+=2) {
		sum += BIG_TO_HOST16(*(uint16_t *)(data + i));
	}
	if(length & 1) {
		uint16_t tmp = 0;
		memcpy(&tmp,data+length-1,1);
		sum += BIG_TO_HOST16(tmp);
	}
	while((uint16_t)(sum >> 16))
		sum = (uint16_t)(sum >> 16) + (uint16_t)(sum & 0xFFFF);
	return HOST_TO_BIG16(~sum);
}

void icmp_receive_echo_request(struct net_dev *nd, struct net_packet *netpacket, union ipv4_address src,
		struct icmp_packet *packet, int len)
{
	uint32_t rest = BIG_TO_HOST32(packet->rest);
	TRACE(0, "[icmp]: got echo request from %x (%d %d)\n", (rest >> 16) & 0xffff, rest & 0xFFFF, src.address);
	int put = 0;
	if(netpacket->flags & NP_FLAG_NOWR) {
		/* we aren't allowed to screw with this packet, so we have to construct a new one */
		/* TODO: this is really slow, is there a better way? */
		struct net_packet *new_packet = net_packet_create(0, 0);
		struct icmp_packet *new_header = (void *)(new_packet->data + ((addr_t)packet - (addr_t)netpacket->data));
		
		new_packet->network_header = (void *)(new_packet->data + ((addr_t)netpacket->network_header - (addr_t)netpacket->data));
		new_packet->length = netpacket->length;
		
		memcpy(new_packet->network_header, netpacket->network_header, len + sizeof(struct ipv4_header));

		netpacket = new_packet;
		packet = new_header;
		put = 1;
	}
	
	packet->type = 0;
	struct ipv4_header *header = netpacket->network_header;
	header->dest_ip = header->src_ip;
	struct sockaddr s;
	net_iface_get_netaddr(nd, AF_INET, &s);
	memcpy(&header->src_ip, s.sa_data + 2, 4);
	ipv4_enqueue_packet(netpacket, header);
	if(put)
		net_packet_put(netpacket, 0);
}

void icmp_receive_packet(struct net_dev *nd, struct net_packet *netpacket, 
		union ipv4_address src, struct icmp_packet *packet, int len)
{
	uint16_t cs = packet->checksum;
	packet->checksum = 0;
	uint16_t calced = icmp_calc_checksum(packet, len);
	TRACE(0, "[icmp]: trace: rec packet from %x. type=%x, code=%x. len = %d\n", src.address, packet->type, packet->code, len);
	if(calced != cs) {
		TRACE(0, "[icmp]: got invalid checksum\n");
		return;
	}
	switch(packet->type) {
		case 8:
			if(packet->code == 0)
				icmp_receive_echo_request(nd, netpacket, src, packet, len);
	}
}

