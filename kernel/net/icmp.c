#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/ipv4.h>
#include <sea/net/icmp.h>
#include <sea/vsprintf.h>

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
	packet->type = 0;
	struct ipv4_header *header = netpacket->network_header;
	header->dest_ip = header->src_ip;
	ipv4_enqueue_packet(netpacket, header);
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

