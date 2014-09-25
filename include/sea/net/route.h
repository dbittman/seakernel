#ifndef __SEA_NET_ROUTE_H
#define __SEA_NET_ROUTE_H

#include <sea/net/ipv4.h>
#include <sea/net/interface.h>
#include <sea/ll.h>

#define ROUTE_FLAG_HOST    1
#define ROUTE_FLAG_DEFAULT 2
#define ROUTE_FLAG_UP      4
#define ROUTE_FLAG_GATEWAY 8

struct route {
	union ipv4_address gateway, destination;
	uint32_t netmask;
	int flags;
	struct net_dev *interface;

	struct llistnode *node;
};

struct route *net_route_select_entry(union ipv4_address addr);
void net_route_add_entry(struct route *r);
void net_route_del_entry(struct route *r);
int net_route_find_del_entry(uint32_t dest, struct net_dev *nd);
#endif

