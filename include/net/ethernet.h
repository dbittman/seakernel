#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

struct ethernet_header {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t type;
};

#endif
