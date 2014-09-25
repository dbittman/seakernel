#ifndef __SEA_NET_UDP_H
#define __SEA_NET_UDP_H

#include <sea/types.h>

extern struct socket_calls socket_calls_udp;

struct udp_header {
	uint16_t src_port;
	uint16_t dest_port;
	uint16_t length;
	uint16_t checksum;
};

#endif

