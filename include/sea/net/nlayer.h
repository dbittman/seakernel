#ifndef __SEA_NET_NLAYER_H
#define __SEA_NET_NLAYER_H

#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/fs/socket.h>

#define NLAYER_FLAG_HW_BROADCAST 1

void net_nlayer_receive_from_dlayer(struct net_dev *nd, struct net_packet *packet, sa_family_t sa_family, void *payload);
int net_nlayer_resolve_hwaddr(struct net_dev *nd, int ethertype, uint8_t *netaddr, uint8_t *hwaddr, int flags);

#endif

