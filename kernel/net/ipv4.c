#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/ipv4.h>
#include <sea/net/icmp.h>
#include <sea/net/route.h>

void ipv4_receive_packet(struct net_dev *nd, struct ipv4_packet *packet)
{
	/* check if we are to accept this packet */
}

int ipv4_send_packet(struct ipv4_packet *packet, union ipv4_address dest)
{
	
	struct net_dev *nd;
	struct route *r = net_route_get_entry(dest);
	if(!r)
		return -1;//TODO: NETWORK_UNREACHABLE;
	nd = r->interface;
	if(r->flags & ROUTE_FLAG_GATEWAY) {
		/* get hw address of gateway (via ARP) */
	} else {
		/* get hw address of destination (via ARP) */
	}
	/*
	ipv4_finish_constructing_packet(packet)

	struct ethernet_header eh;
	uint8_t dest[6];
	dest <- hw_addr;
	ethernet_construct_header(&eh, nd->mac, dest, ETHERTYPE_IPV4);
	ethernet_send_packet(nd, &eh, (unsigned char *)packet, sizeof(*packet));

	*/

	return 0;
}

