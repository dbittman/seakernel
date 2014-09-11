#ifndef __SEA_NET_IPV4SOCK_H
#define __SEA_NET_IPV4SOCK_H
#include <sea/fs/socket.h>
#include <sea/net/packet.h>
#include <sea/net/ipv4.h>

extern struct socket_calls socket_calls_rawipv4;

void ipv4_copy_to_sockets(struct net_packet *packet, struct ipv4_header *header);

#endif

