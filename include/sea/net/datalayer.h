#ifndef __SEA_NET_DATALAYER_H
#define __SEA_NET_DATALAYER_H

#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>

struct data_layer_protocol {
	int flags;
	void (*receive)(struct net_dev *, struct net_packet *);
	void (*send)(struct net_dev *, struct net_packet *, sa_family_t, uint8_t hwdest[6], int payload_len);
};

void net_data_send(struct net_dev *nd, struct net_packet *packet, sa_family_t, uint8_t dest[6], int payload_len);
void net_data_receive(struct net_dev *nd, struct net_packet *packet);
struct data_layer_protocol *net_data_get_protocol(int hwtype);
void net_data_unregister_protocol(int hwtype);
void net_data_register_protocol(int hwtype, struct data_layer_protocol *p);

#endif

