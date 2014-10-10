#include <sea/kernel.h>
#include <sea/string.h>
#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/nlayer.h>
#include <sea/net/arp.h>

#include <sea/fs/socket.h>

#include <sea/errno.h>

struct nlayer_protocol arp = {
	.flags = 0,
	.receive = arp_receive_packet,
	.send = 0,
};

static struct nlayer_protocol *protocols[PF_MAX] = {
	[PF_ARP]  = &arp,
};

void net_nlayer_register_protocol(sa_family_t p, struct nlayer_protocol *np)
{
	protocols[p] = np;
}

void net_nlayer_unregister_protocol(sa_family_t p)
{
	protocols[p] = 0;
}

void net_nlayer_receive_from_dlayer(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, void *payload)
{
	packet->data_header = packet->data;
	struct nlayer_protocol *p = protocols[sa_family];
	if(p && p->receive)
		p->receive(nd, packet, payload);
}

int net_nlayer_send_packet(void *payload, size_t len, struct sockaddr *dest, struct sockaddr *src, sa_family_t sa_family, int prot)
{

	struct nlayer_protocol *p = protocols[sa_family];
	if(p && p->send)
		return p->send(payload, len, dest, src, prot);
	return -ENOTSUP;
}

void net_nlayer_init()
{
	memcpy(protocols, 0, sizeof(protocols));
	protocols[PF_ARP] = &arp;
}

