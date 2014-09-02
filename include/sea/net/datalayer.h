#ifndef __SEA_NET_DATALAYER_H
#define __SEA_NET_DATALAYER_H

#include <sea/net/interface.h>
#include <sea/net/packet.h>

void net_data_send(struct net_dev *nd, struct net_packet *packet, int etype, uint8_t dest[6], int payload_len);
void net_data_receive(struct net_dev *nd, struct net_packet *packet);

#endif

