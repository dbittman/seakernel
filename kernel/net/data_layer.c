#include <sea/asm/system.h>
#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/net/nlayer.h>
#include <sea/net/datalayer.h>

#include <sea/fs/socket.h>

static struct data_layer_protocol *protocols[NET_HWTYPE_MAX];

void net_data_register_protocol(int hwtype, struct data_layer_protocol *p)
{
	protocols[hwtype] = p;
}

void net_data_unregister_protocol(int hwtype)
{
	protocols[hwtype] = 0;
}

struct data_layer_protocol *net_data_get_protocol(int hwtype)
{
	return protocols[hwtype];
}

void net_data_send(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, uint8_t dest[6], int payload_len)
{
	struct data_layer_protocol *dlp = net_data_get_protocol(nd->hw_type);
	if(dlp && dlp->send)
		dlp->send(nd, packet, sa_family, dest, payload_len);
}

void net_data_receive(struct net_dev *nd, struct net_packet *packet)
{
	struct data_layer_protocol *dlp = net_data_get_protocol(nd->hw_type);
	if(dlp && dlp->receive)
		dlp->receive(nd, packet);
}

