#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/ethertype.h>
#include <sea/net/nlayer.h>

#include <sea/net/arp.h>
#include <sea/net/ipv4.h>

#include <sea/fs/socket.h>

#include <sea/errno.h>

void net_nlayer_receive_from_dlayer(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, void *payload)
{
	packet->data_header = packet->data;
	switch(sa_family) {
		case AF_INET:
			ipv4_receive_packet(nd, packet, payload);
			break;
		case AF_ARP:
			arp_receive_packet(nd, packet, payload);
			break;
	}
}

int net_nlayer_resolve_hwaddr(struct net_dev *nd, int ethertype, uint8_t *netaddr, uint8_t *hwaddr, int flags)
{
	if(flags & NLAYER_FLAG_HW_BROADCAST) {
		memset(hwaddr, 0xFF, 6);
	} else {
		if(arp_lookup(ETHERTYPE_IPV4, netaddr, hwaddr) == -ENOENT) {
			/* no idea where the destination is! Send an ARP request. ARP handles multiple
			 * requests to the same address. */
			arp_send_request(nd, ETHERTYPE_IPV4, netaddr, 4);
			return -ENOENT;
		}
	}
	return 0;
}

