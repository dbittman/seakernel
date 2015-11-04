#ifndef __SEA_NET_ROUTE_H
#define __SEA_NET_ROUTE_H

#include <sea/net/interface.h>
#include <sea/lib/linkedlist.h>

#define ROUTE_FLAG_HOST    1
#define ROUTE_FLAG_DEFAULT 2
#define ROUTE_FLAG_UP      4
#define ROUTE_FLAG_GATEWAY 8

struct route {
	uint32_t gateway, destination;
	uint32_t netmask;
	int flags;
	struct net_dev *interface;

	struct linkedentry node;
};

struct route *net_route_select_entry(uint32_t addr);
void net_route_add_entry(struct route *r);
void net_route_del_entry(struct route *r);
int net_route_find_del_entry(uint32_t dest, struct net_dev *nd);
#endif

